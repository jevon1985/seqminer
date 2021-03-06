\name{rvmeta.readCovByRange}
\alias{rvmeta.readCovByRange}
\title{Read covariance by range from METAL-format files.}
\usage{
rvmeta.readCovByRange(covFile, tabixRange)
}
\arguments{
  \item{covFile}{character, a covariance file (rvtests
  outputs using --meta cov)}

  \item{tabixRange}{character, a text indicating which
  range in the VCF file to extract. e.g. 1:100-200}
}
\value{
a matrix of covariance within given range
}
\description{
Read covariance by range from METAL-format files.
}
\examples{
covFileName = system.file("rvtests/rvtest.MetaCov.assoc.gz", package = "seqminer")
cfh <- rvmeta.readCovByRange(covFileName, "1:196621007-196716634")
}
\seealso{
http://zhanxw.com/seqminer/ for online manual and examples
}

