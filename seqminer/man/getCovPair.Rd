\name{getCovPair}
\alias{getCovPair}
\title{Extract pair of positions by ranges}
\usage{
getCovPair(covData, rangeList1, rangeList2)
}
\arguments{
  \item{covData}{a covariance matrix with positions as
  dimnames}

  \item{rangeList1}{character specify a range}

  \item{rangeList2}{character specify a range}
}
\value{
a covariance matrix covFileName =
system.file("rvtests/rvtest.MetaCov.assoc.gz", package =
"seqminer") cfh <- rvmeta.readCovByRange(covFileName,
"1:196621007-196716634") rangeList1 <-
"1:196621007-196700000" rangeList2 <-
"1:196700000-196716634"
}
\description{
Extract pair of positions by ranges
}

