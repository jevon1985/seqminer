\name{tabix.createIndex.vcf}
\alias{tabix.createIndex.vcf}
\title{Create tabix index for bgzipped VCF file}
\usage{
tabix.createIndex.vcf(bgzipVcfFile)
}
\arguments{
  \item{bgzipVcfFile}{character, input vcf file}
}
\description{
Create tabix index for bgzipped VCF file
}
\examples{
fileName = system.file("vcf/all.anno.filtered.extract.vcf.gz", package = "seqminer")
tabix.createIndex.vcf(fileName)
}
\seealso{
http://zhanxw.com/seqminer/ for online manual and examples
}

