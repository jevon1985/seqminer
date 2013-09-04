#include "rvMetaLoader.h"
#include "Maximum.h"

#include <string>
#include <map>
#include <vector>
#include <set>

#include "VCFUtil.h"
#include "tabix.h"
#include "TabixReader.h"
#include "R_CPP_interface.h"

#include <R.h>

#include "GeneLoader.h"

/**
 * @param out will be a concatenated @param in separated by @param sep
 */
void set2string(const std::set<std::string>& in,
                std::string* out,
                const char sep) {
  out->clear();
  std::set<std::string>::const_iterator iter;
  for (iter = in.begin();
       iter != in.end();
       ++iter) {
    if (!out->empty()) {
      out->push_back(sep);
    };
    out->append(*iter);
  };
}

class MetaFileFormat{
 public:
  /// check if all the necessary headers are set up
  virtual bool isComplete() const = 0 ;
  void setHeader(const std::map<std::string, int>& header) {
    std::map<std::string, int>::const_iterator iter = header.begin();
    for ( ;
          iter != header.end();
          ++iter) {
      if (setHeader(iter->first, iter->second) < 0) {
        REprintf("Problem when using the header [ %s ] for column [ %d ]\n", iter->first.c_str(), iter->second);
      }
    }
  }
  int setHeader(const std::string& key, const int& val) {
    if (val < 0) return -1;
    if (get(key) >= 0) // duplicate
      return -1;
    data[key] = val;
  }
  int get(const std::string& key) {
    std::string k = toupper(key);
    std::map<std::string, int>::const_iterator it = data.find(k);

    if (it != data.end()) {
      return it->second;
    }

    // check synonym
    if (synonym.count(k) != 0) {
      const std::set<std::string> & s = synonym.find(k)->second;
      std::set<std::string>::const_iterator i;
      for ( i = s.begin();
            i != s.end();
            ++i) {
        if (data.find(*i) != data.end()) {
          return data.find(*i)->second;
        }
      }
    }

    missingKey.insert(key);
    return -1;
  }

  int peakHeader(const std::string& fn, std::map<std::string, int>* header) {
    header->clear();
    LineReader lr(fn);
    std::vector<std::string> fd;
    std::string line;
    while(lr.readLine(&line)) {
      stringNaturalTokenize(line, "\t ", &fd);
      if (fd.empty()) continue;
      if (fd[0][0] == '#' && fd[0] != "#CHROM") continue;
      // strip out the '#' prefix
      if (fd[0][0] == '#') {
        fd[0] = fd[0].substr(1, fd[0].size() - 1);
      }
      for (size_t i = 0; i < fd.size(); ++i ) {
        if (header->count(fd[i]) ) {
          REprintf("Duplicatd header [ %s ]\n", fd[i].c_str());
          continue;
        }
        (*header) [toupper(fd[i])] = i;
      }
      return 0;
    };
    return -1;
  }
  int open(const std::string& fn) {
    return peakHeader(fn, &data);
  }
  void dump() {
    REprintf("Missing header:\n");
    for (std::set<std::string, int>::const_iterator it = missingKey.begin();
         it != missingKey.end();
         ++it) {
      REprintf("[ %s ] \n", it->c_str());
    }
    REprintf("Known header:\n");
    for (std::map<std::string, int>::const_iterator it = data.begin();
         it != data.end();
         ++it) {

      REprintf("[ %s ] => [ %d ]\n", it->first.c_str(), it->second);
    }
    REprintf("Synonym headers:\n");
    for (std::map<std::string, std::set<std::string> >::const_iterator it = synonym.begin();
         it != synonym.end();
         ++it) {
      REprintf("[ %s ] => ", it->first.c_str());

      const std::set<std::string> & s = synonym.find(it->first)->second;
      std::set<std::string>::const_iterator i;
      for ( i = s.begin();
            i != s.end();
            ++i) {
        REprintf("[ %s ] ", i->c_str());
      }
      REprintf("\n");
    }
  };
  /// add synonym of s1 and s2
  int addSynonym(const std::string& key1, const std::string& key2) {
    std::string s1 = toupper(key1);
    std::string s2 = toupper(key2);
    if (s1 == s2) return 0;
    if (synonym.find(s1) != synonym.end() ) { // already synonym
      if (synonym[s1].count(s2) != 0) {
        return 0;
      }
    }
    if (synonym.find(s2) != synonym.end() ) {
      if (synonym[s2].count(s1) != 0) {
        return 0;
      }
    }

    // add synonym
    {
      synonym[s1].insert(s2);
      const std::set<std::string> & s = synonym.find(s1)->second;
      std::set<std::string>::const_iterator i;
      for ( i = s.begin();
            i != s.end();
            ++i) {
        synonym[*i].insert(s2);
      }
    }
    {
      synonym[s2].insert(s1);
      const std::set<std::string> & s = synonym.find(s2)->second;
      std::set<std::string>::const_iterator i;
      for ( i = s.begin();
            i != s.end();
            ++i) {
        synonym[*i].insert(s1);
      }
    }
    return 0;
  }

 private:
  std::map<std::string, int> data;
  std::set<std::string> missingKey;
  std::map<std::string, std::set <std::string> > synonym; // key: word val: set of synonym of the word
};


/**
   CHROM   POS     REF     ALT     N_INFORMATIVE   AF      INFORMATIVE_ALT_AC      CALL_RATE       HWE_PVALUE      N_REF   N_HET   N_ALT   U_STAT  SQRT_V_STAT     ALT_EFFSIZE     PVALUE
*/
class PvalFileFormat: public MetaFileFormat {
 public:
  PvalFileFormat() {
    addSynonym("AF", "ALL_AF");
  }
  bool isComplete() const {
    return true;
  }
};
/**
   CHROM   START_POS       END_POS NUM_MARKER      MARKER_POS      COV
*/
class CovFileFormat: public MetaFileFormat {
 public:
  CovFileFormat() {
    addSynonym("CURRENT_POS", "START_POS");
    addSynonym("MARKERS_IN_WINDOW", "MARKER_POS");
    addSynonym("COV_MATRICES", "COV");
    addSynonym("CURRENT_POS", "END_POS");
  }
  bool isComplete() const {
    return true;
  }
};

/**
 * Read @param, for each variant in @param range, put each location to @param location under the key @param gene
 * e.g.: locate[gene1][1:100] = 0, locate[gene1][1:120] = 1, ....
 */
void addLocationPerGene(const std::string& gene,
                        const std::string& range,
                        const std::string& fn,
                        OrderedMap< std::string, std::map< std::string, int> >* location) {
  TabixReader tr(fn);
  tr.addRange(range);
  tr.mergeRange();
  std::string line;
  std::vector<std::string> fd;
  std::string key;
  int val;
  while (tr.readLine(&line)) {
    stringNaturalTokenize(line, "\t ", &fd);
    if (fd.size() < 2) continue;
    key = fd[0] + ":" + fd[1];
    if ( (*location)[gene].count(key)) // not update existing variant
      continue;
    val = (*location)[gene].size();
    (*location)[gene][key] = val;
  }
} // end addLocationPerGene

/**
 * We will make sure locations are sorted
 * @param locations: key is 1:1000, value is index
 */
void sortLocationPerGene(std::map< std::string, int>* locations) {
  int i = 0;
  for (std::map<std::string, int>::iterator it = locations->begin();
       it != locations->end();
       ++it) {
    it->second = i ++;
    // Rprintf("%s - %d\n", it ->first.c_str(), it->second);
  }
} // end sortLocationPerGene

/**
 * @return how many covariates in the given @param column in a given file @param fn
 * e.g. in the @param column, we listed covXX:covXZ::covZZ
 * this function examines the dimension of Z.
 * return 0 meaning the given @param column does not have such information
 * return other value, if found.
 */
size_t findCovariateDimension(const std::string& fn, int column) {
  size_t ret = 0;
  LineReader lr(fn);
  std::string line;
  std::vector<std::string> fd;  
  while(lr.readLine(&line)) {
    if (line.empty() || line[0] == '#') continue;
    if (line.substr(0, 5) == "CHROM") continue;
    break;
  }
        
  stringNaturalTokenize(line, "\t ", &fd);
  if (column >= fd.size() ) {
    return ret;
  }
  // REprintf("all_cov = %s\n", line.c_str());
  line = fd[column];
  stringNaturalTokenize(line, ":", &fd);
  if (fd.size() != 3) // covXX, covXZ, and covZZ
    return ret;
  
  line = fd[1]; // covXZ
  // REprintf("covXZ = %s\n", line.c_str());
  stringNaturalTokenize(line, ",", &fd);
  ret = fd.size();

  return ret;
}

#define RET_REF_INDEX 0
#define RET_ALT_INDEX 1
#define RET_NSAMPLE_INDEX 2
#define RET_AF_INDEX 3
#define RET_AC_INDEX 4
#define RET_CALLRATE_INDEX 5
#define RET_HWE_INDEX 6
#define RET_NREF_INDEX 7
#define RET_NHET_INDEX 8
#define RET_NALT_INDEX 9
#define RET_USTAT_INDEX 10
#define RET_SQRTVSTAT_INDEX 11
#define RET_EFFECT_INDEX 12
#define RET_PVAL_INDEX 13
#define RET_COV_INDEX 14
#define RET_POS_INDEX 15
#define RET_ANNO_INDEX 16
#define RET_COV_XZ_INDEX 17
#define RET_COV_ZZ_INDEX 18
#define RET_HWE_CASE_INDEX 19
#define RET_HWE_CTRL_INDEX 20

SEXP impl_rvMetaReadData(SEXP arg_pvalFile, SEXP arg_covFile,
                         const OrderedMap< std::string, std::string>& geneRange) {

  int numAllocated = 0;
  SEXP ret = R_NilValue;

  // load by position
  std::vector<std::string> FLAG_pvalFile, FLAG_covFile;
  extractStringArray(arg_pvalFile, &FLAG_pvalFile);
  extractStringArray(arg_covFile, &FLAG_covFile);

  if (FLAG_pvalFile.size() != FLAG_covFile.size()){
    if (FLAG_covFile.size() == 0) {
      Rprintf("Skip loading covaraince file!\n");
    } else {
      Rprintf("Unequal size between score file and cov file!\n");
      Rprintf("Quitting...");
      return ret;
    }
  }

  int nStudy = FLAG_pvalFile.size();

  // store the order of the gene
  std::map< std::string, int> geneIndex;
  {
    int len = geneRange.size();
    for (int i = 0; i < len; ++i) {
      const std::string& key = geneRange.keyAt(i);
      geneIndex[key] = i;
    }
  }

  
  // for each gene, find its all position location
  typedef OrderedMap< std::string, std::map< std::string, int> > GeneLocationMap;
  GeneLocationMap geneLocationMap;
  for (size_t idx = 0; idx < geneRange.size(); ++idx) {
    const std::string& key = geneRange.keyAt(idx);
    const std::string& value = geneRange.valueAt(idx);    
    for (int i = 0; i < nStudy; ++i) {
      addLocationPerGene(key, value, FLAG_pvalFile[i], &geneLocationMap);
    }
    for (int i = 0; i < nStudy; ++i) {
      sortLocationPerGene(& (geneLocationMap[key]));
    }
  };
  std::map<std::string, std::set<std::string> > posAnnotationMap;


  // initial return results
  // result[gene][chrom, pos, maf...][nstudy]
  int nGene = geneLocationMap.size();
  Rprintf("%d gene/region to be extracted.\n", nGene);

  PROTECT(ret = allocVector(VECSXP, nGene));
  numAllocated ++;
  SEXP geneNames;
  PROTECT(geneNames = allocVector(STRSXP, nGene));
  numAllocated ++;
  {
    for (int i = 0; i < geneLocationMap.size(); ++i) {
    // for ( GeneLocationMap::iterator iter = geneLocationMap.begin();
    //       iter != geneLocationMap.end() ; ++iter){
      // Rprintf("assign gene name: %s\n", iter->first.c_str());
      SET_STRING_ELT(geneNames, i, mkChar(geneLocationMap.keyAt(i).c_str()));
      geneIndex[geneLocationMap.keyAt(i).c_str()] = i;
    }
  }
  setAttrib(ret, R_NamesSymbol, geneNames);

  // create n, maf, p, cov...
  // REprintf("create n, maf, p, cov..\n");
  std::vector<std::string> names;
  names.push_back("ref");
  names.push_back("alt");
  names.push_back("nSample");
  names.push_back("af");
  names.push_back("ac");
  names.push_back("callrate");
  names.push_back("hwe");
  names.push_back("nref");
  names.push_back("nhet");
  names.push_back("nalt");
  names.push_back("ustat");
  names.push_back("vstat");
  names.push_back("effect");
  names.push_back("pVal");
  names.push_back("cov");
  names.push_back("pos");
  names.push_back("anno");
  names.push_back("covXZ");
  names.push_back("covZZ");
  names.push_back("hweCase");
  names.push_back("hweCtrl");
  
  // REprintf("create zDims\n");
  std::vector<size_t> zDims(FLAG_covFile.size(), 0);
  // GeneLocationMap::iterator iter = geneLocationMap.begin();
  for ( int i = 0;
        i < geneLocationMap.size();
        ++i) {
    // REprintf("i = %d\n", i);
    // REprintf("names.size() = %zu\n", names.size());
    //        iter != geneLocationMap.end() ; ++iter, ++i){
    SEXP s = VECTOR_ELT(ret, i);
    numAllocated += createList(names.size(), &s); // a list with 10 elements: ref, alt, n, maf, stat, direction, p, cov, pos, anno
    numAllocated += setListNames(names, &s);

    SEXP ref, alt, n, af, ac, callRate, hwe, nref, nhet, nalt, ustat, vstat, effect, p, cov, pos, anno, covXZ, covZZ, hweCase, hweCtrl;
    numAllocated += createList(nStudy, &ref);
    numAllocated += createList(nStudy, &alt);
    numAllocated += createList(nStudy, &n);
    numAllocated += createList(nStudy, &af);
    numAllocated += createList(nStudy, &ac);
    numAllocated += createList(nStudy, &callRate);
    numAllocated += createList(nStudy, &hwe);
    numAllocated += createList(nStudy, &nref);
    numAllocated += createList(nStudy, &nhet);
    numAllocated += createList(nStudy, &nalt);
    numAllocated += createList(nStudy, &ustat);
    numAllocated += createList(nStudy, &vstat);
    numAllocated += createList(nStudy, &effect);
    numAllocated += createList(nStudy, &p);
    numAllocated += createList(nStudy, &cov);
    numAllocated += createList(nStudy, &pos);
    numAllocated += createList(nStudy, &anno);
    numAllocated += createList(nStudy, &covXZ);
    numAllocated += createList(nStudy, &covZZ);
    numAllocated += createList(nStudy, &hweCase);
    numAllocated += createList(nStudy, &hweCtrl);
    
    int npos = geneLocationMap.valueAt(i).size();
    // std::vector<size_t> zDims(FLAG_covFile.size(), 0);
    // REprintf("npos= %d\n", npos);
    for (int j = 0; j < nStudy; ++j) {
      SEXP t;
      numAllocated += createStringArray(npos, &t);
      initStringArray(t);
      SET_VECTOR_ELT(ref, j, t);

      numAllocated += createStringArray(npos, &t);
      initStringArray(t);
      SET_VECTOR_ELT(alt, j, t);

      numAllocated += createIntArray(npos, &t);
      initIntArray(t);
      SET_VECTOR_ELT(n, j, t);

      numAllocated += createDoubleArray(npos, &t);
      initDoubleArray(t);
      SET_VECTOR_ELT(af, j, t);

      numAllocated += createIntArray(npos, &t);
      initIntArray(t);
      SET_VECTOR_ELT(ac, j, t);

      numAllocated += createDoubleArray(npos, &t);
      initDoubleArray(t);
      SET_VECTOR_ELT(callRate, j, t);

      numAllocated += createDoubleArray(npos, &t);
      initDoubleArray(t);
      SET_VECTOR_ELT(hwe, j, t);

      numAllocated += createIntArray(npos, &t);
      initIntArray(t);
      SET_VECTOR_ELT(nref, j, t);

      numAllocated += createIntArray(npos, &t);
      initIntArray(t);
      SET_VECTOR_ELT(nhet, j, t);

      numAllocated += createIntArray(npos, &t);
      initIntArray(t);
      SET_VECTOR_ELT(nalt, j, t);

      numAllocated += createDoubleArray(npos, &t);
      initDoubleArray(t);
      SET_VECTOR_ELT(ustat, j, t);

      numAllocated += createDoubleArray(npos, &t);
      initDoubleArray(t);
      SET_VECTOR_ELT(vstat, j, t);

      numAllocated += createDoubleArray(npos, &t);
      initDoubleArray(t);
      SET_VECTOR_ELT(effect, j, t);

      numAllocated += createDoubleArray(npos, &t);
      initDoubleArray(t);
      SET_VECTOR_ELT(p, j, t);

      // Rprintf("Create double array %d for study %d\n", npos * npos, j);
      if (FLAG_covFile.empty()) {
        /// if skip covFile, then just set cov to be 1 by 1 matrix of NA
        numAllocated += createDoubleArray(1, &t);
        numAllocated += setDim(1, 1, &t);
      } else {
        numAllocated += createDoubleArray(npos*npos, &t);
        numAllocated += setDim(npos, npos, &t);
      }
      initDoubleArray(t);
      SET_VECTOR_ELT(cov, j, t);

      // allocate memory for cov_xz
      if (FLAG_covFile.size() != 0) {
        CovFileFormat covHeader;
        if (covHeader.open(FLAG_covFile[j]) < 0 ){
          REprintf("Study [ %s ] does not have valid file header \n", FLAG_covFile[j].c_str());
          continue;
        }
        const int COV_FILE_COV_COL = covHeader.get("COV");
        zDims[j] = findCovariateDimension(FLAG_covFile[j], COV_FILE_COV_COL);
        const int zDim = zDims[j];
        // REprintf("npos = %d, zDim = %d\n", npos, zDim);
        if (FLAG_covFile.empty()) {
          /// if skip covFile, then just set cov to be 1 by 1 matrix of NA
          numAllocated += createDoubleArray(1, &t);
          numAllocated += setDim(1, 1, &t);
        } else {
          numAllocated += createDoubleArray(npos*zDim, &t);
          numAllocated += setDim(npos, zDim, &t);
        }
        initDoubleArray(t);
        SET_VECTOR_ELT(covXZ, j, t);

        // allocate memory for cov_zz
        if (FLAG_covFile.empty()) {
          /// if skip covFile, then just set cov to be 1 by 1 matrix of NA
          numAllocated += createDoubleArray(1, &t);
          numAllocated += setDim(1, 1, &t);
        } else {
          numAllocated += createDoubleArray(zDim*zDim, &t);
          numAllocated += setDim(zDim, zDim, &t);
        }
        initDoubleArray(t);
        SET_VECTOR_ELT(covZZ, j, t);
      }

      // allocate memory for hweCase, hweCtrl
      numAllocated += createDoubleArray(npos, &t);
      initDoubleArray(t);
      SET_VECTOR_ELT(hweCase, j, t);

      numAllocated += createDoubleArray(npos, &t);
      initDoubleArray(t);
      SET_VECTOR_ELT(hweCtrl, j, t);
    } // end looping study
    numAllocated += createStringArray(npos, &pos);
    initStringArray(pos);

    numAllocated += createStringArray(npos, &anno);
    initStringArray(anno);

    SET_VECTOR_ELT(s, RET_REF_INDEX, ref);
    SET_VECTOR_ELT(s, RET_ALT_INDEX, alt);
    SET_VECTOR_ELT(s, RET_NSAMPLE_INDEX, n);
    SET_VECTOR_ELT(s, RET_AF_INDEX, af);
    SET_VECTOR_ELT(s, RET_AC_INDEX, ac);
    SET_VECTOR_ELT(s, RET_CALLRATE_INDEX, callRate);
    SET_VECTOR_ELT(s, RET_HWE_INDEX, hwe);
    SET_VECTOR_ELT(s, RET_NREF_INDEX, nref);
    SET_VECTOR_ELT(s, RET_NHET_INDEX, nhet);
    SET_VECTOR_ELT(s, RET_NALT_INDEX, nalt);
    SET_VECTOR_ELT(s, RET_USTAT_INDEX, ustat);
    SET_VECTOR_ELT(s, RET_SQRTVSTAT_INDEX, vstat);
    SET_VECTOR_ELT(s, RET_EFFECT_INDEX, effect);
    SET_VECTOR_ELT(s, RET_PVAL_INDEX, p);
    SET_VECTOR_ELT(s, RET_COV_INDEX, cov);
    SET_VECTOR_ELT(s, RET_POS_INDEX, pos);
    SET_VECTOR_ELT(s, RET_ANNO_INDEX, anno);
    SET_VECTOR_ELT(s, RET_COV_XZ_INDEX, covXZ);
    SET_VECTOR_ELT(s, RET_COV_ZZ_INDEX, covZZ);
    SET_VECTOR_ELT(s, RET_HWE_CASE_INDEX, hweCase);
    SET_VECTOR_ELT(s, RET_HWE_CTRL_INDEX, hweCtrl);
        
    SET_VECTOR_ELT(ret, i, s);
  };
  // REprintf("finish allocate memory\n");
                                 
  // return results
  Rprintf("Read score tests...\n");
  // read pval file and fill in values
  for (int study = 0; study < nStudy; ++study) {
    Rprintf("In study %d\n", study);
    // read header
    PvalFileFormat pvalHeader;
    if (pvalHeader.open(FLAG_pvalFile[study]) < 0 ){
      REprintf("Study [ %s ] does not have valid file header \n", FLAG_pvalFile[study].c_str());
      continue;
    }

    const int PVAL_FILE_CHROM_COL = pvalHeader.get("CHROM");
    const int PVAL_FILE_POS_COL = pvalHeader.get("POS");
    const int PVAL_FILE_REF_COL = pvalHeader.get("REF");
    const int PVAL_FILE_ALT_COL = pvalHeader.get("ALT");
    const int PVAL_FILE_NINFORMATIVE_COL = pvalHeader.get("N_INFORMATIVE");
    const int PVAL_FILE_AF_COL = pvalHeader.get("AF");
    const int PVAL_FILE_AC_COL = pvalHeader.get("INFORMATIVE_ALT_AC");
    const int PVAL_FILE_CALLRATE_COL = pvalHeader.get("CALL_RATE");
    const int PVAL_FILE_HWE_COL = pvalHeader.get("HWE_PVALUE");
    const int PVAL_FILE_NREF_COL = pvalHeader.get("N_REF");
    const int PVAL_FILE_NHET_COL = pvalHeader.get("N_HET");
    const int PVAL_FILE_NALT_COL = pvalHeader.get("N_ALT");
    const int PVAL_FILE_USTAT_COL = pvalHeader.get("U_STAT");
    const int PVAL_FILE_SQRTVSTAT_COL = pvalHeader.get("SQRT_V_STAT");
    const int PVAL_FILE_EFFECT_COL = pvalHeader.get("ALT_EFFSIZE");
    const int PVAL_FILE_PVAL_COL = pvalHeader.get("PVALUE");
    const int PVAL_FILE_ANNO_COL = pvalHeader.get("ANNO");

    Maximum maximum;
    const int PVAL_FILE_MIN_COLUMN_NUM = maximum
                                         .add(PVAL_FILE_CHROM_COL)
                                         .add(PVAL_FILE_POS_COL)
                                         .add(PVAL_FILE_REF_COL)
                                         .add(PVAL_FILE_ALT_COL)
                                         .add(PVAL_FILE_NINFORMATIVE_COL)
                                         .add(PVAL_FILE_AF_COL)
                                         .add(PVAL_FILE_AC_COL)
                                         .add(PVAL_FILE_CALLRATE_COL)
                                         .add(PVAL_FILE_HWE_COL)
                                         .add(PVAL_FILE_NREF_COL)
                                         .add(PVAL_FILE_NHET_COL)
                                         .add(PVAL_FILE_NALT_COL)
                                         .add(PVAL_FILE_USTAT_COL)
                                         .add(PVAL_FILE_SQRTVSTAT_COL)
                                         .add(PVAL_FILE_EFFECT_COL)
                                         .add(PVAL_FILE_PVAL_COL)
                                         .add(PVAL_FILE_ANNO_COL)
                                         .max();
                                         
    if (PVAL_FILE_CHROM_COL < 0 ||
        PVAL_FILE_POS_COL < 0 ||
        PVAL_FILE_REF_COL < 0 ||
        PVAL_FILE_ALT_COL < 0 ||
        PVAL_FILE_NINFORMATIVE_COL < 0 ||
        PVAL_FILE_AF_COL < 0 ||
        PVAL_FILE_AC_COL < 0 ||
        PVAL_FILE_CALLRATE_COL < 0 ||
        PVAL_FILE_HWE_COL < 0 ||
        PVAL_FILE_NREF_COL < 0 ||
        PVAL_FILE_NHET_COL < 0 ||
        PVAL_FILE_NALT_COL < 0 ||
        PVAL_FILE_USTAT_COL < 0 ||
        PVAL_FILE_SQRTVSTAT_COL < 0 ||
        PVAL_FILE_EFFECT_COL < 0 ||
        PVAL_FILE_PVAL_COL < 0 ) {
      REprintf("Study [ %s ] does not have all necessary headers\n", FLAG_pvalFile[study].c_str());
    }

    // loop per gene
    for (size_t idx = 0; idx < geneRange.size(); ++idx) {
      const std::string& gene = geneRange.keyAt(idx);
      const std::string& range = geneRange.valueAt(idx);    

      // if (geneLocationMap.find(gene) == geneLocationMap.end()) continue;
      if (!geneLocationMap.find(gene) ) continue;
      const std::map< std::string, int>& location2idx = geneLocationMap[gene];

      std::set<std::string> processedSite ;
      TabixReader tr(FLAG_pvalFile[study]);
      tr.addRange(range);
      std::string line;
      std::vector< std::string> fd;
      // temp values
      std::string p; // meaning position
      int tempInt;
      double tempDouble;
      while( tr.readLine(&line) ){
        stringNaturalTokenize(line, " \t", &fd);
        if (fd.size() <= PVAL_FILE_MIN_COLUMN_NUM) continue;
        p = fd[PVAL_FILE_CHROM_COL];
        p += ':';
        p += fd[PVAL_FILE_POS_COL];
        if (processedSite.count(p) == 0) {
          processedSite.insert(p);
        } else{
          Rprintf("Position %s appeared more than once, skipping...\n", p.c_str());
          continue;
        }

        SEXP u, v, s;
        // std::string& gene = locationGeneMap[p];
        /// if (FLAG_gene.count(gene) == 0) continue;
        // if (geneLocationMap.count(gene) == 0 ||
        //     geneLocationMap[gene].count(p) == 0) continue; // skip non existing position
        // int idx = geneLocationMap[gene][p];
        int idx = location2idx.find(p)->second;

        // Rprintf("working on index %d, with position %s\n", idx, p.c_str());
        u = VECTOR_ELT(ret, geneIndex[gene]);
        v = VECTOR_ELT(u, RET_REF_INDEX);
        s = VECTOR_ELT(v, study); // ref
        SET_STRING_ELT(s, idx, mkChar(fd[PVAL_FILE_REF_COL].c_str()));

        // u = VECTOR_ELT(ret, geneIndex[gene]);
        v = VECTOR_ELT(u, RET_ALT_INDEX);
        s = VECTOR_ELT(v, study); // alt
        SET_STRING_ELT(s, idx, mkChar(fd[PVAL_FILE_ALT_COL].c_str()));

        if ( str2int(fd[PVAL_FILE_NINFORMATIVE_COL], &tempInt) ) {
          // u = VECTOR_ELT(ret, geneIndex[gene]);
          v = VECTOR_ELT(u, RET_NSAMPLE_INDEX);
          s = VECTOR_ELT(v, study); // n
          INTEGER(s)[idx] = tempInt;
        }

        if ( str2double(fd[PVAL_FILE_AF_COL], &tempDouble) ) {
          v = VECTOR_ELT(u, RET_AF_INDEX);
          s = VECTOR_ELT(v, study); // af
          REAL(s)[idx] = tempDouble;
        }

        if ( str2int(fd[PVAL_FILE_AC_COL], &tempInt) ) {
          v = VECTOR_ELT(u, RET_AC_INDEX);
          s = VECTOR_ELT(v, study); // ac
          INTEGER(s)[idx] = tempInt;
        }

        if ( str2double(fd[PVAL_FILE_CALLRATE_COL], &tempDouble) ) {
          v = VECTOR_ELT(u, RET_CALLRATE_INDEX);
          s = VECTOR_ELT(v, study); // callRate
          REAL(s)[idx] = tempDouble;
        }

        // hwe field may have one pvalue or three pvalue (all:case:control)
        std::vector<std::string> hwePvalues;
        // REprintf("hwe = %s\n", fd[PVAL_FILE_HWE_COL].c_str());
        stringTokenize(fd[PVAL_FILE_HWE_COL], ":", &hwePvalues);
        if (!hwePvalues.empty()) {
          if ( str2double(hwePvalues[0], &tempDouble) ) {
            v = VECTOR_ELT(u, RET_HWE_INDEX);
            s = VECTOR_ELT(v, study); // hwe
            REAL(s)[idx] = tempDouble;
          }
          if (hwePvalues.size() == 3) { // hwe_all:hwe_case:hwe_ctrl
            if ( str2double(hwePvalues[1], &tempDouble) ) {
              v = VECTOR_ELT(u, RET_HWE_CASE_INDEX);
              s = VECTOR_ELT(v, study); // hwe
              REAL(s)[idx] = tempDouble;
            }
            if ( str2double(hwePvalues[2], &tempDouble) ) {
              v = VECTOR_ELT(u, RET_HWE_CTRL_INDEX);
              s = VECTOR_ELT(v, study); // hwe
              REAL(s)[idx] = tempDouble;
            }
          }
        }  else {
          REprintf("HWE column has incorrect value [ %s ]", fd[PVAL_FILE_HWE_COL].c_str());
        }
        
        if ( str2int(fd[PVAL_FILE_NREF_COL], &tempInt) ) {
          v = VECTOR_ELT(u, RET_NREF_INDEX);
          s = VECTOR_ELT(v, study); // nref
          INTEGER(s)[idx] = tempInt;
        }

        if ( str2int(fd[PVAL_FILE_NHET_COL], &tempInt) ) {
          v = VECTOR_ELT(u, RET_NHET_INDEX);
          s = VECTOR_ELT(v, study); // nhet
          INTEGER(s)[idx] = tempInt;
        }

        if ( str2int(fd[PVAL_FILE_NALT_COL], &tempInt) ) {
          v = VECTOR_ELT(u, RET_NALT_INDEX);
          s = VECTOR_ELT(v, study); // nalt
          INTEGER(s)[idx] = tempInt;
        }

        if ( str2double( fd[PVAL_FILE_USTAT_COL], & tempDouble)) {
          v = VECTOR_ELT(u, RET_USTAT_INDEX);
          s = VECTOR_ELT(v, study); // ustat
          REAL(s)[idx] = tempDouble;
        }

        if ( str2double( fd[PVAL_FILE_SQRTVSTAT_COL], & tempDouble)) {
          v = VECTOR_ELT(u, RET_SQRTVSTAT_INDEX);
          s = VECTOR_ELT(v, study); // vstat
          REAL(s)[idx] = tempDouble;
        }

        if ( str2double( fd[PVAL_FILE_EFFECT_COL], & tempDouble)) {
          v = VECTOR_ELT(u, RET_EFFECT_INDEX);
          s = VECTOR_ELT(v, study); // effect
          REAL(s)[idx] = tempDouble;
        }

        if ( str2double(fd[PVAL_FILE_PVAL_COL], & tempDouble)) {
          // Rprintf("Set pval index");
          v = VECTOR_ELT(u, RET_PVAL_INDEX);
          s = VECTOR_ELT(v, study); // pval
          REAL(s)[idx] = tempDouble;
        };

        if (PVAL_FILE_ANNO_COL >= 0 && (int)fd.size() >= PVAL_FILE_ANNO_COL) {
          const std::string& s = fd[PVAL_FILE_ANNO_COL];
          if (!posAnnotationMap[p].count(s))  {
            posAnnotationMap[p].insert(s);
          }
        }

      } // end while
    }
    Rprintf("Done read score file: %s\n", FLAG_pvalFile[study].c_str());
  } // end loop by study
  // fill in position and annotation
  // iter = geneLocationMap.begin();
  for ( int i = 0;
        i < geneLocationMap.size();
        ++i) {
    // iter != geneLocationMap.end() ; ++iter, ++i){
    const std::map<std::string, int>& loc2idx = geneLocationMap.valueAt(i);
    SEXP u = VECTOR_ELT(ret, geneIndex[geneLocationMap.keyAt(i)]);
    SEXP pos = VECTOR_ELT(u, RET_POS_INDEX);
    SEXP anno = VECTOR_ELT(u, RET_ANNO_INDEX);

    for (std::map<std::string, int>::const_iterator it = loc2idx.begin();
         it != loc2idx.end();
         ++it) {
      int idx = it->second;
      // if (locations.count(it->first)) {
      //   SET_STRING_ELT(pos, idx, mkChar(it->first.c_str()));
      // }
      SET_STRING_ELT(pos, idx, mkChar(it->first.c_str()));

      if (posAnnotationMap.count(it->first)) {
        std::string ret;
        set2string(posAnnotationMap[it->first], &ret, ',');
        SET_STRING_ELT(anno, idx, mkChar(ret.c_str()));
      };
    }
  }

  // read cov file and record pos2idx
  if (FLAG_covFile.empty()) {
    Rprintf("Skip reading cov files ... \n");
  } else {
    Rprintf("Read cov files ... \n");
    std::vector<std::string> pos;
    std::vector<std::string> covArray;
    std::vector<std::string> cov;
    std::vector<std::string> covXZ;
    std::vector<std::string> covZZ;

    for (int study = 0; study < nStudy; ++study) {
      // parse header
      CovFileFormat covHeader;
      if (covHeader.open(FLAG_covFile[study]) < 0 ){
        REprintf("Study [ %s ] does not have valid file header \n", FLAG_covFile[study].c_str());
        continue;
      }

      const int COV_FILE_CHROM_COL = covHeader.get("CHROM");
      const int COV_FILE_START_COL = covHeader.get("START_POS");
      const int COV_FILE_END_COL = covHeader.get("END_POS");
      const int COV_FILE_NUM_MARKER_COL = covHeader.get("NUM_MARKER");
      const int COV_FILE_POS_COL = covHeader.get("MARKER_POS");
      const int COV_FILE_COV_COL = covHeader.get("COV");

      Maximum maximum;
      const int COV_FILE_MIN_COLUMN_NUM = maximum
                                          .add(COV_FILE_CHROM_COL)
                                          .add(COV_FILE_START_COL)
                                          .add(COV_FILE_END_COL)
                                          .add(COV_FILE_NUM_MARKER_COL)
                                          .add(COV_FILE_POS_COL)
                                          .add(COV_FILE_COV_COL)
                                          .max();
      if (COV_FILE_CHROM_COL < 0 ||
          COV_FILE_START_COL < 0 ||
          COV_FILE_END_COL < 0 ||
          COV_FILE_NUM_MARKER_COL < 0 ||
          COV_FILE_POS_COL < 0 ||
          COV_FILE_COV_COL < 0 ) {
        REprintf("Study [ %s ] does not have all necessary headers\n", FLAG_covFile[study].c_str());
      }
      
      
      // loop per gene
      for (size_t idx = 0; idx < geneRange.size(); ++idx) {
        const std::string& gene = geneRange.keyAt(idx);
        const std::string& range = geneRange.valueAt(idx);    

        // if (geneLocationMap.find(gene) == geneLocationMap.end()) continue;
        if (!geneLocationMap.find(gene)) continue;
        const std::map< std::string, int>& location2idx = geneLocationMap[gene];

        std::set<std::string> processedSite ;

        // REprintf("Read file %s ", FLAG_covFile[study].c_str());
        TabixReader tr(FLAG_covFile[study]);
        tr.addRange(range);
        tr.mergeRange();
        std::string line;
        std::vector< std::string> fd;
        while( tr.readLine(&line) ){
          // REprintf("line = %s\n", line.c_str());

          stringNaturalTokenize(line, " \t", &fd);
          if (fd.size() <= COV_FILE_MIN_COLUMN_NUM) continue;
          
          const std::string& chrom = fd[COV_FILE_CHROM_COL];
          // REprintf("pos: %s\n", fd[COV_FILE_COV_COL].c_str());
          stringNaturalTokenize(fd[COV_FILE_POS_COL], ',', &pos);
          stringNaturalTokenize(fd[COV_FILE_COV_COL], ':', &covArray);          
          stringNaturalTokenize(covArray[0], ',', &cov);

          if (pos.empty()) {
            REprintf("No position found in [ %s ]\n", line.c_str());
            continue;
          }
          if (covArray.empty()) {
            REprintf("No covariance matrices found in [ %s ]\n", line.c_str());
            continue;
          }
          if (pos.size() != cov.size()) {
            REprintf("position length does not equal to covariance length\n");
            continue;
          }

          int covLen = location2idx.size();
          SEXP u, v, s;
          u = VECTOR_ELT(ret, geneIndex[gene]);
          v = VECTOR_ELT(u, RET_COV_INDEX);  // cov is the 6th element in the list
          s = VECTOR_ELT(v, study);

          std::string pi = chrom + ":" + pos[0];
          int posi = location2idx.find(pi)->second;
          // REprintf("Pos %s = %d, covLen = %d\n", pi.c_str(), posi, covLen);
          std::string pj;
          double tmp;
          for (size_t j = 0; j < pos.size(); ++j) {
            pj = chrom + ":" + pos[j];
            if (location2idx.count(pj) == 0) {
              continue;
            }
            int posj = location2idx.find(pj)->second;
            // REprintf("i = %d, j = %d\n", posi, posj);
            if (str2double(cov[j], &tmp)) {
              REAL(s) [posi * covLen + posj] = tmp;
              REAL(s) [posj * covLen + posi] = tmp;
            }
          }
          
          // check if we have covXZ and covZZ fields
          if (covArray.size() == 1) continue;

          // now loading covXZ, covZZ
          REprintf("read covXZ\n");
          v = VECTOR_ELT(u, RET_COV_XZ_INDEX);  // cov is the 6th element in the list
          s = VECTOR_ELT(v, study);
          stringNaturalTokenize(covArray[1], ',', &covXZ);
          // REprintf("covXZ = %s\n", covArray[1].c_str());
          int zDim = zDims[study];
          if (covXZ.size() != zDim) {
            REprintf("CovXZ has wrong dimension (Covariance column = [ %s ])\n", fd[COV_FILE_COV_COL].c_str() );
          }
          for (size_t j = 0; j < zDim; ++j) {
            pj = chrom + ":" + pos[0];
            // REprintf("pj = %s, covXZ[j] = %s\n", pj.c_str(), covXZ[j].c_str());
            if (location2idx.count(pj) == 0) {
              continue;
            }
            int posj = location2idx.find(pj)->second;
            if (str2double(covXZ[j], &tmp)) {
              // REprintf("j = %d, covLen = %d, posj = %d, tmp = %g\n", j, covLen, posj, tmp);
              REAL(s) [ j* covLen + posj] = tmp;
            }
          }
          // REprintf("read covZZ\n");
          v = VECTOR_ELT(u, RET_COV_ZZ_INDEX);  // cov is the 6th element in the list
          s = VECTOR_ELT(v, study);
          stringNaturalTokenize(covArray[2], ',', &covZZ);
          if (zDim * (zDim + 1) / 2 != covZZ.size()) {
            REprintf("CovXZ has wrong dimension (Covariance column = [ %s ])\n", fd[COV_FILE_COV_COL].c_str() );
          }
          // REprintf("covZZ = %s\n", covArray[2].c_str());
          // Z is stored like this:
          // 1
          // 2, 3,
          // 4, 5, 6
          // 7, 8, 9, 10
          // REprintf("covLen = %d, zDim = %d\n", covLen, zDim);
          int idx = 0;
          for (int i = 0; i < zDim; ++i) {
            for (int j = 0; j <= i; ++j) {
            if (str2double(covZZ[idx], &tmp)) {
              REAL(s) [ i * zDim + j ]  = tmp;
              REAL(s) [ j * zDim + i ]  = tmp;
            }
            ++idx;
            }
          }
        } // end tabixReader
      } // end loop by gene
      Rprintf("Done read cov file: %s\n", FLAG_covFile[study].c_str());
    } // end loop by study
  }

  Rprintf("Finished calculation.\n");
  UNPROTECT(numAllocated);

  return ret;
} // impl_rvMetaReadData

SEXP impl_rvMetaReadDataByRange(SEXP arg_pvalFile, SEXP arg_covFile, SEXP arg_range) {
  std::vector<std::string> FLAG_range;
  extractStringArray(arg_range, &FLAG_range);

  OrderedMap< std::string, std::string> geneRange ;
  char key[100] = "";
  for (size_t i = 0; i < FLAG_range.size(); ++i) {
    sprintf(key, "Range%d", (int) (i + 1));
    geneRange[key] = FLAG_range[i];  
  }

  return impl_rvMetaReadData(arg_pvalFile, arg_covFile, geneRange);
} // impl_rvMetaReadDataByRange

SEXP impl_rvMetaReadDataByGene(SEXP arg_pvalFile, SEXP arg_covFile, SEXP arg_geneFile, SEXP arg_gene) {
  // load gene
  std::string FLAG_geneFile;
  std::set<std::string> FLAG_gene;
  extractString(arg_geneFile, &FLAG_geneFile);
  extractStringSet(arg_gene, &FLAG_gene);

  OrderedMap< std::string, std::string> geneRange ;
  loadGeneFile(FLAG_geneFile, FLAG_gene, &geneRange);

  return impl_rvMetaReadData(arg_pvalFile, arg_covFile, geneRange);
} // impl_rvMetaReadDataByGene


/**
 * @return a covariance matrix
 */
SEXP impl_readCovByRange(SEXP arg_covFile, SEXP arg_range) {
  int numAllocated = 0;
  SEXP ret = R_NilValue;

  // load by position
  std::string FLAG_covFile;
  extractString(arg_covFile, &FLAG_covFile);
  std::string FLAG_range;
  extractString(arg_range, &FLAG_range);

  // parse header
  CovFileFormat covHeader;
  if (covHeader.open(FLAG_covFile) < 0 ){
    REprintf("File [ %s ] does not have valid file header \n", FLAG_covFile.c_str());
    return ret;
  }

  const int COV_FILE_CHROM_COL = covHeader.get("CHROM");
  const int COV_FILE_START_COL = covHeader.get("START_POS");
  const int COV_FILE_END_COL = covHeader.get("END_POS");
  const int COV_FILE_NUM_MARKER_COL = covHeader.get("NUM_MARKER");
  const int COV_FILE_POS_COL = covHeader.get("MARKER_POS");
  const int COV_FILE_COV_COL = covHeader.get("COV");

  Maximum maximum;
  const int COV_FILE_MIN_COLUMN_NUM = maximum
                                      .add(COV_FILE_CHROM_COL)
                                      .add(COV_FILE_START_COL)
                                      .add(COV_FILE_END_COL)
                                      .add(COV_FILE_NUM_MARKER_COL)
                                      .add(COV_FILE_POS_COL)
                                      .add(COV_FILE_COV_COL)
                                      .max();
  
  if (COV_FILE_CHROM_COL < 0 &&
      COV_FILE_START_COL < 0 &&
      COV_FILE_END_COL < 0 &&
      COV_FILE_NUM_MARKER_COL < 0 &&
      COV_FILE_POS_COL < 0 &&
      COV_FILE_COV_COL < 0 ) {
    REprintf("File [ %s ] does not have all necessary headers\n", FLAG_covFile.c_str());
  }

  // Rprintf("open %s\n", FLAG_covFile.c_str());
  tabix_t* t = ti_open(FLAG_covFile.c_str(), 0);
  if (t == 0) {
    REprintf("Cannot open %s file!\n", FLAG_covFile.c_str());
    return ret;
  }
  // Rprintf("open OK\n");

  if (ti_lazy_index_load(t) < 0) {
    REprintf("[tabix] failed to load the index file.\n");
    return ret;
  }

  std::string line;
  std::vector<std::string> fd;
  std::string chrom;
  std::map< std::string, int> pos2idx;
  std::vector<int> positionPerRow;
  std::vector<std::string> position;

  // these are used each line
  std::vector<std::string> cov;
  std::vector<int> pos;
  std::vector<std::string> fdPos;
  std::vector<std::string> fdCov;

  int lineNo = 0;
  ti_iter_t iter;
  int tid, beg, end;
  const char* s;
  int len;
  // Rprintf("begin parse %s ..\n", FLAG_range.c_str());
  if (ti_parse_region(t->idx, FLAG_range.c_str(), &tid, &beg, &end) == 0) {
    // Rprintf("parse OK\n");
    iter = ti_queryi(t, tid, beg, end);
    while ((s = ti_read(t, iter, &len)) != 0) {
      // fputs(s, stdout); fputc('\n', stdout);
      // Rprintf("%s\n", s);
      line = s;
      stringTokenize(line, "\t", &fd);
      if (fd.size() <= COV_FILE_MIN_COLUMN_NUM) continue;
      // if (fd[COV_FILE_NUM_MARKER_COL] == "1") continue; // only self covariance
      lineNo ++;

      if (chrom.empty()){
        chrom = fd[0];
      } else{
        if (chrom != fd[0]){
          REprintf("chromosome does not match %s and %s.\n", chrom.c_str(), fd[0].c_str());
          return ret;
        }
      }
      stringTokenize(fd[COV_FILE_POS_COL], ',', &fdPos);
      stringTokenize(fd[COV_FILE_COV_COL], ',', &fdCov);
      if (fdPos.empty()) {
        REprintf("No position found in [ %s ]\n", line.c_str());
        continue;
      }
      if (fdCov.empty()) {
        REprintf("No covariance matrices found in [ %s ]\n", line.c_str());
        continue;
      }
      if (fdPos.size() != fdCov.size()) {
        REprintf("Malformated pos and cov line\n");
        continue;
      }

      int considerPos = 0;
      for (size_t i = 0; i < fdPos.size(); ++i){
        // Rprintf("fdPos[%zu] = %s\n", i, fdPos[i].c_str());
        int p = atoi(fdPos[i]);
        if (p > end) continue;
        // consider in-range positions only (tabix already consider p >= beg)
        if (pos2idx.count(fdPos[i]) == 0) {
          pos2idx[fdPos[i]] = position.size();
          position.push_back(fdPos[i]);
        }
        ++considerPos;
      }
      // REprintf("fdPos.size() = %zu, considerPos = %d\n", fdPos.size(), considerPos);

      // verify positions are in order
      bool inOrder = true;
      int beg = pos2idx[fdPos[0]];
      for (int j = 0; j < considerPos; ++j) {
        if (pos2idx[fdPos[j]] != beg + (int)j) {
          REprintf("The position field is not in order\n");
          REprintf("beg = %d, j = %d, pos2idx[fdPos[j]] = %d, fdPos[j] = %s\n", beg, j, pos2idx[fdPos[j]], fdPos[j].c_str());
          inOrder = false;
          break;
        }
      }
      if (!inOrder) {
        REprintf("Not in order, please check covariance file\n");
        return ret;
      }

      for (int i = 0; i < considerPos; ++i) {
        cov.push_back(fdCov[i]);
      }
      positionPerRow.push_back(considerPos);
      // REprintf("considerPos = %d\n", considerPos);
    }
    ti_iter_destroy(iter);
    // Rprintf("parse end\n");
  }  else {
    REprintf("invalid region: unknown target name or minus interval.\n");
    return ret;
  }
  int retDim = position.size();
  Rprintf("Total %d line loaded, now put them to matrix [ %d x %d ] in R ...\n", lineNo, retDim, retDim);
  // Rprintf("cov.size() = %d\n", (int)cov.size());
  // // Rprintf("pos2idx.size() = %zu \n", pos2idx.size());
  // for (std::map<std::string, int>::const_iterator iter = pos2idx.begin();
  //      iter != pos2idx.end();
  //      ++iter) {
  //   // Rprintf("%s -> %d\n", iter->first.c_str(), iter->second);
  // }

  // init R matrix
  numAllocated += createDoubleArray(retDim * retDim, &ret);
  initDoubleArray(ret);

  // store results
  int offset = 0;
  double c;
  for (int i = 0; i < retDim; ++i) {
    const int width = positionPerRow[i];
    for (int j = 0; j < width; ++j) {
      const int row = i;
      const int col = i + j;

      // Rprintf("%d: %d %d = %g\n", i, row, col, cov[i]);
      if (str2double(cov[offset+j], &c)) {
        REAL(ret)[row * retDim + col] = c;
        REAL(ret)[col * retDim + row] = c;
      }
    }
    offset += width;
  }
  ti_close(t);

  // set dim info
  numAllocated += setDim(retDim, retDim, &ret);
  // set matrix label
  SEXP rowName;
  PROTECT(rowName=allocVector(STRSXP, retDim));
  numAllocated += 1;
  std::string label;
  for (size_t i = 0; i < position.size() ; ++i) {
    label = chrom;
    label += ':';
    label += position[i];
    SET_STRING_ELT(rowName, i, mkChar(label.c_str()));
  }

  SEXP dimnames;
  PROTECT(dimnames = allocVector(VECSXP, 2));
  numAllocated += 1;
  SET_VECTOR_ELT(dimnames, 0, rowName);
  SET_VECTOR_ELT(dimnames, 1, rowName);
  setAttrib(ret, R_DimNamesSymbol, dimnames);

  UNPROTECT(numAllocated);
  return ret;
} // impl_readCovByRange

/**
 * @return 0 if success; -1 if not oK
 */
int parsePosition(const std::string& range,
                  std::string* chrom,
                  int* beg,
                  int* end){

  std::string r;
  r = chopChr(range);
  size_t i = r.find(':');
  if (i == std::string::npos)
    return -1;

  *chrom = r.substr(0, i);

  size_t j = r.find('-', i+1);
  if (j == std::string::npos) { // 1:100
    *beg = atoi(r.substr(i, r.size() - i));
    *end = INT_MAX;
    return 0;
  }
  // 1:100-200
  //  ^   ^   ^
  //  i   j
  //  1   5   8
  *beg = atoi(r.substr(i+1, j - i - 1));
  *end = atoi(r.substr(j+1, r.size() - j));
  return 0;
}
/**
 * @param arg_scoreFile: single variant score test file
 * @return a data frame of
 */
SEXP impl_readScoreByRange(SEXP arg_scoreFile, SEXP arg_range) {
  int numAllocated = 0;
  SEXP ret = R_NilValue;

  // load by position
  std::string FLAG_scoreFile;
  extractString(arg_scoreFile, &FLAG_scoreFile);
  std::string FLAG_range;
  extractString(arg_range, &FLAG_range);

  // read header
  PvalFileFormat pvalHeader;
  if (pvalHeader.open(FLAG_scoreFile) < 0 ){
    REprintf("File [ %s ] does not have valid file header \n", FLAG_scoreFile.c_str());
    return ret;
  }

  const int PVAL_FILE_CHROM_COL = pvalHeader.get("CHROM");
  const int PVAL_FILE_POS_COL = pvalHeader.get("POS");
  const int PVAL_FILE_REF_COL = pvalHeader.get("REF");
  const int PVAL_FILE_ALT_COL = pvalHeader.get("ALT");
  const int PVAL_FILE_NINFORMATIVE_COL = pvalHeader.get("N_INFORMATIVE");
  const int PVAL_FILE_AF_COL = pvalHeader.get("AF");
  const int PVAL_FILE_AC_COL = pvalHeader.get("INFORMATIVE_ALT_AC");
  const int PVAL_FILE_CALLRATE_COL = pvalHeader.get("CALL_RATE");
  const int PVAL_FILE_HWE_COL = pvalHeader.get("HWE_PVALUE");
  const int PVAL_FILE_NREF_COL = pvalHeader.get("N_REF");
  const int PVAL_FILE_NHET_COL = pvalHeader.get("N_HET");
  const int PVAL_FILE_NALT_COL = pvalHeader.get("N_ALT");
  const int PVAL_FILE_USTAT_COL = pvalHeader.get("U_STAT");
  const int PVAL_FILE_SQRTVSTAT_COL = pvalHeader.get("SQRT_V_STAT");
  const int PVAL_FILE_EFFECT_COL = pvalHeader.get("ALT_EFFSIZE");
  const int PVAL_FILE_PVAL_COL = pvalHeader.get("PVALUE");
  const int PVAL_FILE_ANNO_COL = pvalHeader.get("ANNO");
  const int PVAL_FILE_ANNOFULL_COL = pvalHeader.get("ANNOFULL");

    Maximum maximum;
    const int PVAL_FILE_MIN_COLUMN_NUM = maximum
                                         .add(PVAL_FILE_CHROM_COL)
                                         .add(PVAL_FILE_POS_COL)
                                         .add(PVAL_FILE_REF_COL)
                                         .add(PVAL_FILE_ALT_COL)
                                         .add(PVAL_FILE_NINFORMATIVE_COL)
                                         .add(PVAL_FILE_AF_COL)
                                         .add(PVAL_FILE_AC_COL)
                                         .add(PVAL_FILE_CALLRATE_COL)
                                         .add(PVAL_FILE_HWE_COL)
                                         .add(PVAL_FILE_NREF_COL)
                                         .add(PVAL_FILE_NHET_COL)
                                         .add(PVAL_FILE_NALT_COL)
                                         .add(PVAL_FILE_USTAT_COL)
                                         .add(PVAL_FILE_SQRTVSTAT_COL)
                                         .add(PVAL_FILE_EFFECT_COL)
                                         .add(PVAL_FILE_PVAL_COL)
                                         .add(PVAL_FILE_ANNO_COL)
                                         .max();
  
  if (PVAL_FILE_CHROM_COL < 0 ||
      PVAL_FILE_POS_COL < 0 ||
      PVAL_FILE_REF_COL < 0 ||
      PVAL_FILE_ALT_COL < 0 ||
      PVAL_FILE_NINFORMATIVE_COL < 0 ||
      PVAL_FILE_AF_COL < 0 ||
      PVAL_FILE_AC_COL < 0 ||
      PVAL_FILE_CALLRATE_COL < 0 ||
      PVAL_FILE_HWE_COL < 0 ||
      PVAL_FILE_NREF_COL < 0 ||
      PVAL_FILE_NHET_COL < 0 ||
      PVAL_FILE_NALT_COL < 0 ||
      PVAL_FILE_USTAT_COL < 0 ||
      PVAL_FILE_SQRTVSTAT_COL < 0 ||
      PVAL_FILE_EFFECT_COL < 0 ||
      PVAL_FILE_PVAL_COL < 0 ) {
    REprintf("File [ %s ] does not have all necessary headers\n", FLAG_scoreFile.c_str());
    pvalHeader.dump();
    return ret;
  }

  // // parse region
  // std::string chrom;
  // int beg;
  // int end;
  // parsePosition(FLAG_range, &chrom, &beg, &end);
  // Rprintf("chrom = %s, beg = %d, end = %d", chrom.c_str(), beg, end);

  // set up return values
  std::vector <std::string> fd;
  std::string line;
  TabixReader tr(FLAG_scoreFile);
  int fieldLen = -1;
  std::vector <int> position;
  std::vector <std::string> ref;
  std::vector <std::string> alt;
  std::vector <std::string> nsample;
  std::vector <std::string> af;
  std::vector <std::string> ac;
  std::vector <std::string> callRate;
  std::vector <std::string> hwe;
  std::vector <std::string> nref;
  std::vector <std::string> nhet;
  std::vector <std::string> nalt;
  std::vector <std::string> ustat;
  std::vector <std::string> vstat;
  std::vector <std::string> effect;
  std::vector <std::string> pval;
  std::vector <std::string> anno;
  std::vector <std::string> annoFull;
  tr.addRange(FLAG_range);
  //Rprintf("begin to read range %s ..\n", FLAG_range.c_str());
  while (tr.readLine(&line)) {
    // Rprintf("read a line: %s\n", line.c_str());
    stringNaturalTokenize(line, "\t ", &fd);
    if (fd.size() <= PVAL_FILE_MIN_COLUMN_NUM) continue;
    int pos = atoi(fd[PVAL_FILE_POS_COL]);

    // check consistent column number
    if (fieldLen < 0) {
      fieldLen = fd.size();
    } else if (fieldLen != (int)fd.size()) {
      REprintf("Inconsistent field length at line [ %s ]. \n", line.c_str());
      return ret;
    }

    position.push_back(pos);
    ref.push_back(fd[PVAL_FILE_REF_COL]);
    alt.push_back(fd[PVAL_FILE_ALT_COL]);
    nsample.push_back(fd[PVAL_FILE_NINFORMATIVE_COL]);
    af.push_back(fd[PVAL_FILE_AF_COL]);
    ac.push_back(fd[PVAL_FILE_AC_COL]);
    callRate.push_back(fd[PVAL_FILE_CALLRATE_COL]);
    hwe.push_back(fd[PVAL_FILE_HWE_COL]);
    nref.push_back(fd[PVAL_FILE_NREF_COL]);
    nhet.push_back(fd[PVAL_FILE_NHET_COL]);
    nalt.push_back(fd[PVAL_FILE_NALT_COL]);
    ustat.push_back(fd[PVAL_FILE_USTAT_COL]);
    vstat.push_back(fd[PVAL_FILE_SQRTVSTAT_COL]);
    effect.push_back(fd[PVAL_FILE_EFFECT_COL]); ///
    pval.push_back(fd[PVAL_FILE_PVAL_COL]);
    if ((int)fd.size() > PVAL_FILE_ANNO_COL && PVAL_FILE_ANNO_COL >= 0) {
      anno.push_back(fd[PVAL_FILE_ANNO_COL]);
    } else {
      anno.push_back("");
    }
    if ((int)fd.size() > PVAL_FILE_ANNOFULL_COL && PVAL_FILE_ANNOFULL_COL >= 0) {
      annoFull.push_back(fd[PVAL_FILE_ANNOFULL_COL]);
    } else {
      annoFull.push_back("");
    }
  };

  if (fieldLen < 0) {
    REprintf("No valid input line read, please check your file [ %s ]\n", FLAG_scoreFile.c_str());
    return ret;
  };

  //REprintf("Construct return values\n");
  int retListLen;
  if (anno.size() ) {
    retListLen = 17; // hard coded number
  } else {
    retListLen = 15;
  }
  PROTECT(ret = allocVector(VECSXP, retListLen));
  numAllocated ++;

  std::vector<std::string> listNames;
  int retListIdx = 0;
  numAllocated += storeResult(position, ret, retListIdx++);
  numAllocated += storeResult(ref, ret, retListIdx++);
  numAllocated += storeResult(alt, ret, retListIdx++);
  numAllocated += storeIntResult(nsample, ret, retListIdx++);
  numAllocated += storeDoubleResult(af, ret, retListIdx++);
  numAllocated += storeIntResult(ac, ret, retListIdx++);
  numAllocated += storeDoubleResult(callRate, ret, retListIdx++);
  numAllocated += storeResult(hwe, ret, retListIdx++); // hwe may have three number (all:case:ctrl)
  numAllocated += storeIntResult(nref, ret, retListIdx++);
  numAllocated += storeIntResult(nhet, ret, retListIdx++);
  numAllocated += storeIntResult(nalt, ret, retListIdx++);
  numAllocated += storeDoubleResult(ustat, ret, retListIdx++);
  numAllocated += storeDoubleResult(vstat, ret, retListIdx++);
  numAllocated += storeDoubleResult(effect, ret, retListIdx++);
  numAllocated += storeDoubleResult(pval, ret, retListIdx++);

  listNames.push_back("pos");
  listNames.push_back("ref");
  listNames.push_back("alt");
  listNames.push_back("nSample");
  listNames.push_back("af");
  listNames.push_back("ac");
  listNames.push_back("callRate");
  listNames.push_back("hwe");
  listNames.push_back("nref");
  listNames.push_back("nhet");
  listNames.push_back("nalt");
  listNames.push_back("ustat");
  listNames.push_back("vstat");
  listNames.push_back("effect");
  listNames.push_back("pVal");

  if (anno.size() ) {
    numAllocated += storeResult(anno, ret, retListIdx++);
    numAllocated += storeResult(annoFull, ret, retListIdx++);
    listNames.push_back("anno");
    listNames.push_back("annoFull");
  }
  SEXP sListNames;
  PROTECT(sListNames = allocVector(STRSXP, listNames.size()));
  numAllocated ++;
  for (unsigned int i = 0; i != listNames.size(); ++i){
    SET_STRING_ELT(sListNames, i, mkChar(listNames[i].c_str()));
  }
  setAttrib(ret, R_NamesSymbol, sListNames);

  UNPROTECT(numAllocated);
  return ret;
} // impl_readScoreByRange