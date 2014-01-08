import FWCore.ParameterSet.Config as cms

from DQMOffline.JetMET.jetMETDQMCleanup_cff import *

#metDQMParameters = cms.PSet(

tcMetAnalyzer = cms.EDAnalyzer("METAnalyzer",

    #
    # Output files
    #
    OutputMEsInRootFile = cms.bool(False),
    OutputFile = cms.string('jetMETMonitoring.root'),
    METType=cms.untracked.string('tc'),

    METCollectionLabel     = cms.InputTag("tcMet"),
    JetCollectionLabel  = cms.InputTag("JetPlusTrackZSPCorJetAntiKt5"),
    
    FolderName = cms.untracked.string("JetMET/MET/"),

    CleaningParameters = cleaningParameters.clone(),

    TriggerResultsLabel  = cms.InputTag("TriggerResults::HLT"),

    HLTPathsJetMB = cms.vstring(),
#    When it is empty, it accepts all the triggers
#    HLTPathsJetMB = cms.vstring(
#        "HLT_L1Jet15","HLT_Jet30","HLT_Jet50","HLT_Jet80","HLT_Jet110","HLT_Jet180",
#        "HLT_DiJetAve15","HLT_DiJetAve30","HLT_DiJetAve50","HLT_DiJetAve70",
#        "HLT_DiJetAve130","HLT_DiJetAve220",
#        "HLT_MinBias"),

    highPtJetTrigger = cms.PSet(
        andOr         = cms.bool( False ),
        dbLabel        = cms.string("JetMETDQMTrigger"),
        hltInputTag    = cms.InputTag( "TriggerResults::HLT" ),
        hltDBKey       = cms.string( 'jetmet_highptjet' ),
        hltPaths       = cms.vstring( 'HLT_PFJet400_v*' ), 
        andOrHlt       = cms.bool( True ),
        errorReplyHlt  = cms.bool( False ),
    ),
    lowPtJetTrigger = cms.PSet(
        andOr         = cms.bool( False ),
        dbLabel        = cms.string("JetMETDQMTrigger"),
        hltInputTag    = cms.InputTag( "TriggerResults::HLT" ),
        hltDBKey       = cms.string( 'jetmet_lowptjet' ),
        hltPaths       = cms.vstring( 'HLT_PFJet80_v*' ), 
        andOrHlt       = cms.bool( True ),
        errorReplyHlt  = cms.bool( False ),
    ),
    minBiasTrigger = cms.PSet(
        andOr         = cms.bool( False ),
        dbLabel        = cms.string("JetMETDQMTrigger"),
        hltInputTag    = cms.InputTag( "TriggerResults::HLT" ),
        hltDBKey       = cms.string( 'jetmet_minbias' ),
        hltPaths       = cms.vstring( 'HLT_L1Tech_BSC_minBias_threshold1_v*' ), 
        andOrHlt       = cms.bool( True ),
        errorReplyHlt  = cms.bool( False ),
    ),
    highMETTrigger = cms.PSet(
        andOr         = cms.bool( False ),
        dbLabel        = cms.string("JetMETDQMTrigger"),
        hltInputTag    = cms.InputTag( "TriggerResults::HLT" ),
        hltDBKey       = cms.string( 'jetmet_highmet' ),
        hltPaths       = cms.vstring( 'HLT_MET400_v*' ), 
        andOrHlt       = cms.bool( True ),
        errorReplyHlt  = cms.bool( False ),
    ),
    #    lowMETTrigger = cms.PSet(
    #        andOr         = cms.bool( False ),
    #        dbLabel        = cms.string("JetMETDQMTrigger"),
    #        hltInputTag    = cms.InputTag( "TriggerResults::HLT" ),
    #        hltDBKey       = cms.string( 'jetmet_lowmet' ),
    #        hltPaths       = cms.vstring( 'HLT_MET120_HBHENoiseFiltered_v*' ), 
    #        andOrHlt       = cms.bool( True ),
    #        errorReplyHlt  = cms.bool( False ),
    #    ),
    eleTrigger = cms.PSet(
        #andOr         = cms.bool( False ),
        dbLabel        = cms.string("JetMETDQMTrigger"),
        hltInputTag    = cms.InputTag( "TriggerResults::HLT" ),
        hltDBKey       = cms.string( 'jetmet_ele' ),
        hltPaths       = cms.vstring( 'HLT_Ele25_CaloIdL_CaloIsoVL_TrkIdVL_TrkIsoVL_v','HLT_Ele25_CaloIdL_CaloIsoVL_TrkIdVL_TrkIsoVL_v4','HLT_Ele25_CaloIdL_CaloIsoVL_TrkIdVL_TrkIsoVL_v5','HLT_Ele25_CaloIdL_CaloIsoVL_TrkIdVL_TrkIsoVL_v6' ), 
        #andOrHlt       = cms.bool( True ),
        errorReplyHlt  = cms.bool( False ),
    ),
    muonTrigger = cms.PSet(
        #andOr         = cms.bool( False ),
        dbLabel        = cms.string("JetMETDQMTrigger"),
        hltInputTag    = cms.InputTag( "TriggerResults::HLT" ),
        hltDBKey       = cms.string( 'jetmet_muon' ),
        hltPaths       = cms.vstring( 'HLT_IsoMu20_v','HLT_IsoMu20_v8','HLT_IsoMu20_v9','HLT_IsoMu20_v10' ), 
        #andOrHlt       = cms.bool( True ),
        errorReplyHlt  = cms.bool( False ),
    ),


    JetIDParams = cms.PSet(
        useRecHits = cms.bool(True),
        hbheRecHitsColl = cms.InputTag("hbhereco"),
        hoRecHitsColl   = cms.InputTag("horeco"),
        hfRecHitsColl   = cms.InputTag("hfreco"),
        ebRecHitsColl   = cms.InputTag("ecalRecHit", "EcalRecHitsEB"),
        eeRecHitsColl   = cms.InputTag("ecalRecHit", "EcalRecHitsEE")
    ),
    
    HcalNoiseRBXCollection     = cms.InputTag("hcalnoise"),
    HBHENoiseFilterResultLabel = cms.InputTag("HBHENoiseFilterResultProducer", "HBHENoiseFilterResult"),
    BeamHaloSummaryLabel       = cms.InputTag("BeamHaloSummary"),    

    HighPtJetThreshold = cms.double(60.),
    LowPtJetThreshold  = cms.double(15.),
    HighMETThreshold   = cms.double(110.),

    pVBin       = cms.int32(100),
    pVMax       = cms.double(100.0),
    pVMin       = cms.double(0.0),

    #    LowMETThreshold    = cms.double(30.),

    verbose     = cms.int32(0),
    printOut    = cms.int32(0),

    etThreshold  = cms.double(2.),
    allHist      = cms.bool(False),
    allSelection = cms.bool(False),
    cleanupSelection = cms.bool(True),
    
    #Parameters set for METAnalyzer --> but only used for TCMET
    InputBeamSpotLabel = cms.InputTag("offlineBeamSpot"),
    InputTrackLabel    = cms.InputTag("generalTracks"),
    InputMuonLabel     = cms.InputTag("muons"),
    InputElectronLabel = cms.InputTag("gedGsfElectrons"),
    InputTCMETValueMap = cms.InputTag("muonTCMETValueMapProducer","muCorrData"),#muonMETValueMapProducer -> calomet vs muonTCMETValueMapProducer
    DCSFilter = cms.PSet(
        DetectorTypes = cms.untracked.string("ecal:hbhe:hf:pixel:sistrip:es:muon"),
        #DebugOn = cms.untracked.bool(True),
        Filter = cms.untracked.bool(True)
        )


)

pfMetAnalyzer = tcMetAnalyzer.clone(
    METType=cms.untracked.string('pf'),
    METCollectionLabel     = cms.InputTag("pfMet"),
    JetCollectionLabel  = cms.InputTag("ak5PFJets"),

)

metAnalyzer = tcMetAnalyzer.clone(
    METType=cms.untracked.string('calo'),
    METCollectionLabel     = cms.InputTag("met"),
    JetCollectionLabel  = cms.InputTag("ak5CaloJets"),
    allHist = cms.bool(True),
    DCSFilter = cms.PSet(
      DetectorTypes = cms.untracked.string("ecal:hbhe:hf"),
      #DebugOn = cms.untracked.bool(True),
      Filter = cms.untracked.bool(True)
    )
)
