\name{rvmeta.readSkewByRange}
\alias{rvmeta.readSkewByRange}
\title{Read skew by range from METAL-format files.}
\usage{
rvmeta.readSkewByRange(skewFile, tabixRange)
}
\arguments{
  \item{skewFile}{character, a skew file (rvtests outputs
  using --meta skew)}

  \item{tabixRange}{character, a text indicating which
  range in the VCF file to extract. e.g. 1:100-200}
}
\value{
an 3-dimensional array of skewness within given range
}
\description{
Read skew by range from METAL-format files.
}
\examples{
skewFileName = system.file("rvtests/rvtest.MetaSkew.assoc.gz", package = "seqminer")
cfh <- rvmeta.readSkewByRange(skewFileName, "1:196621007-196716634")
}
\seealso{
http://zhanxw.com/seqminer/ for online manual and examples
}

