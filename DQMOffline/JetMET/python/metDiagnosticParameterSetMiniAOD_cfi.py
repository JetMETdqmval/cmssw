import FWCore.ParameterSet.Config as cms
import math


multPhiCorr_METDiagnostics = cms.VPSet(
    cms.PSet(
        name=cms.string("h"),
        type=cms.int32(211),
        nbins=cms.double(250),
        nMin=cms.int32(0),
        nMax=cms.int32(7500),
        etaNBins=cms.int32(108),
        etaMin=cms.double(-2.7),
        etaMax=cms.double(2.7),
        phiNBins=cms.int32(160),
        phiMin=cms.double(-math.pi),
        phiMax=cms.double(math.pi),
        ), 
    cms.PSet(
        name=cms.string("h0Barrel"),
        type=cms.int32(130),
        nbins=cms.double(250),
        nMin=cms.int32(0),
        nMax=cms.int32(250),
        etaNBins=cms.int32(32),
        etaMin=cms.double(-1.392),
        etaMax=cms.double(1.392),
        phiNBins=cms.int32(72),
        phiMin=cms.double(-math.pi),
        phiMax=cms.double(math.pi),
        ),
    cms.PSet(
        name=cms.string("h0EndcapPlus"),
        type=cms.int32(130),
        nbins=cms.double(250),
        nMin=cms.int32(0),
        nMax=cms.int32(250),
        etaNBins=cms.int32(12),
        etaMin=cms.double(1.392),
        etaMax=cms.double(3),
        phiNBins=cms.int32(18),
        phiMin=cms.double(-math.pi),
        phiMax=cms.double(math.pi),
        ),
    cms.PSet(
        name=cms.string("h0EndcapMinus"),
        type=cms.int32(130),
        nbins=cms.double(250),
        nMin=cms.int32(0),
        nMax=cms.int32(250),
        etaNBins=cms.int32(12),
        etaMin=cms.double(-3.0),
        etaMax=cms.double(-1.392),
        phiNBins=cms.int32(18),
        phiMin=cms.double(-math.pi),
        phiMax=cms.double(math.pi),
        ),
    cms.PSet(
        name=cms.string("gammaBarrel"),
        type=cms.int32(22),
        nbins=cms.double(125),
        nMin=cms.int32(0),
        nMax=cms.int32(2500),
        etaNBins=cms.int32(170),
        etaMin=cms.double(-1.479),
        etaMax=cms.double(1.479),
        phiNBins=cms.int32(360),
        phiMin=cms.double(-math.pi),
        phiMax=cms.double(math.pi),
        ),
    cms.PSet(
        name=cms.string("gammaEndcapPlus"),
        type=cms.int32(22),
        nbins=cms.double(125),
        nMin=cms.int32(0),
        nMax=cms.int32(750),
        etaNBins=cms.int32(20),
        etaMin=cms.double(1.479),
        etaMax=cms.double(3.0),
        phiNBins=cms.int32(30),
        phiMin=cms.double(-math.pi),
        phiMax=cms.double(math.pi),
        ),
    cms.PSet(
        name=cms.string("gammaEndcapMinus"),
        type=cms.int32(22),
        nbins=cms.double(125),
        nMin=cms.int32(0),
        nMax=cms.int32(750),
        etaNBins=cms.int32(20),
        etaMin=cms.double(-3.0),
        etaMax=cms.double(-1.479),
        phiNBins=cms.int32(30),
        phiMin=cms.double(-math.pi),
        phiMax=cms.double(math.pi),
        ),
    cms.PSet(
        name=cms.string("gammaForwardPlus"),
        type=cms.int32(22),
        nbins=cms.double(50),
        nMin=cms.int32(0),
        nMax=cms.int32(50),
        etaNBins=cms.int32(20),
        etaMin=cms.double(3.0),
        etaMax=cms.double(5.0),
        phiNBins=cms.int32(30),
        phiMin=cms.double(-math.pi),
        phiMax=cms.double(math.pi),
        ),
    cms.PSet(
        name=cms.string("gammaForwardMinus"),
        type=cms.int32(22),
        nbins=cms.double(50),
        nMin=cms.int32(0),
        nMax=cms.int32(50),
        etaNBins=cms.int32(20),
        etaMin=cms.double(-5.0),
        etaMax=cms.double(-3.0),
        phiNBins=cms.int32(30),
        phiMin=cms.double(-math.pi),
        phiMax=cms.double(math.pi),
        ),
    cms.PSet(
        name=cms.string("e"),
        type=cms.int32(11),
        nbins=cms.double(50),
        nMin=cms.int32(0),
        nMax=cms.int32(50),
        etaNBins=cms.int32(27),
        etaMin=cms.double(-2.7),
        etaMax=cms.double(2.7),
        phiNBins=cms.int32(40),
        phiMin=cms.double(-math.pi),
        phiMax=cms.double(math.pi),
        ),
    cms.PSet(
        name=cms.string("mu"),
        type=cms.int32(13),
        nbins=cms.double(50),
        nMin=cms.int32(0),
        nMax=cms.int32(50),
        etaNBins=cms.int32(27),
        etaMin=cms.double(-2.7),
        etaMax=cms.double(2.7),
        phiNBins=cms.int32(40),
        phiMin=cms.double(-math.pi),
        phiMax=cms.double(math.pi),
        ),
    cms.PSet(
        name=cms.string("hHFMinus"),
        type=cms.int32(1),
        nbins=cms.double(125),
        nMin=cms.int32(0),
        nMax=cms.int32(1250),
        etaNBins=cms.int32(11),
        etaMin=cms.double(-5.20),
        etaMax=cms.double(-2.901376),
        phiNBins=cms.int32(18),
        phiMin=cms.double(-math.pi),
        phiMax=cms.double(math.pi),
        ),
    cms.PSet(
        name=cms.string("hHFPlus"),
        type=cms.int32(1),
        nbins=cms.double(125),
        nMin=cms.int32(0),
        nMax=cms.int32(1250),
        etaNBins=cms.int32(11),
        etaMin=cms.double(2.901376),
        etaMax=cms.double(5.20),
        phiNBins=cms.int32(18),
        phiMin=cms.double(-math.pi),
        phiMax=cms.double(math.pi),
        ),
    cms.PSet(
        name=cms.string("egammaHFMinus"),
        type=cms.int32(2),
        nbins=cms.double(125),
        nMin=cms.int32(0),
        nMax=cms.int32(1250),
        etaNBins=cms.int32(11),
        etaMin=cms.double(-5.2),
        etaMax=cms.double(-2.901376),
        phiNBins=cms.int32(18),
        phiMin=cms.double(-math.pi),
        phiMax=cms.double(math.pi),
        ),
    cms.PSet(
        name=cms.string("egammaHFPlus"),
        type=cms.int32(2),
        nbins=cms.double(125),
        nMin=cms.int32(0),
        nMax=cms.int32(1250),
        etaNBins=cms.int32(11),
        etaMin=cms.double(2.901376),
        etaMax=cms.double(5.20),
        phiNBins=cms.int32(18),
        phiMin=cms.double(-math.pi),
        phiMax=cms.double(math.pi),
        ),
    )
