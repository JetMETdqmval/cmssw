/** \class JetAnalyzer
 *
 *  DQM jetMET analysis monitoring
 *
 *  \author F. Chlebana - Fermilab
 *          K. Hatakeyama - Rockefeller University
 *
 *          Jan. '14: modified by
 *
 *          M. Artur Weber
 *          R. Schoefbeck
 *          V. Sordini
 */

#include "DQMOffline/JetMET/interface/JetAnalyzer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Common/interface/TriggerNames.h"
#include "DataFormats/JetReco/interface/CaloJetCollection.h"
#include "DataFormats/JetReco/interface/CaloJet.h"
#include "DataFormats/JetReco/interface/JPTJetCollection.h"
#include "DataFormats/JetReco/interface/PFJetCollection.h"
#include "DataFormats/JetReco/interface/PFJet.h"
#include "DataFormats/Common/interface/Handle.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "DataFormats/L1GlobalTrigger/interface/L1GlobalTriggerReadoutSetupFwd.h"
#include "DataFormats/L1GlobalTrigger/interface/L1GlobalTriggerReadoutSetup.h"
#include "DataFormats/L1GlobalTrigger/interface/L1GlobalTriggerReadoutRecord.h"

#include "FWCore/Framework/interface/ESHandle.h"
#include "DataFormats/Math/interface/deltaR.h"
#include "RecoJets/JetAssociationAlgorithms/interface/JetTracksAssociationDRCalo.h"
#include "DataFormats/Math/interface/Point3D.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h"
#include "TrackingTools/Records/interface/TrackingComponentsRecord.h"
#include "DataFormats/TrackingRecHit/interface/TrackingRecHit.h"
#include "DataFormats/TrackerRecHit2D/interface/SiStripMatchedRecHit2D.h"
#include "DataFormats/TrackerRecHit2D/interface/ProjectedSiStripRecHit2D.h"
#include "DataFormats/TrackerRecHit2D/interface/SiStripRecHit2D.h"
#include "DataFormats/TrackingRecHit/interface/InvalidTrackingRecHit.h"
#include "DataFormats/SiStripCluster/interface/SiStripCluster.h"
#include "DataFormats/DetId/interface/DetId.h"
#include "DataFormats/SiStripDetId/interface/SiStripDetId.h"
#include "CalibFormats/SiStripObjects/interface/SiStripQuality.h"
#include "CondFormats/SiStripObjects/interface/SiStripNoises.h"
#include "CalibFormats/SiStripObjects/interface/SiStripGain.h"
#include "CalibTracker/Records/interface/SiStripQualityRcd.h"
#include "CondFormats/DataRecord/interface/SiStripNoisesRcd.h"
#include "CalibTracker/Records/interface/SiStripGainRcd.h"

#include <string>

using namespace edm;
using namespace reco;
using namespace std;

namespace jetAnalysis {
  
  // Helper class to propagate tracks to the calo surface using the same implementation as the JetTrackAssociator
  class TrackPropagatorToCalo
  {
   public:
    TrackPropagatorToCalo();
    void update(const edm::EventSetup& eventSetup);
    math::XYZPoint impactPoint(const reco::Track& track) const;
   private:
    const MagneticField* magneticField_;
    const Propagator* propagator_;
    uint32_t magneticFieldCacheId_;
    uint32_t propagatorCacheId_;
  };
  /*
  // Helper class to calculate strip signal to noise and manage the necessary ES objects
  class StripSignalOverNoiseCalculator
  {
   public:
   StripSignalOverNoiseCalculator(const std::string& theQualityLabel = std::string(""));
    void update(const edm::EventSetup& eventSetup);
    double signalOverNoise(const SiStripCluster& cluster,
			   const uint32_t& id) const;
    double operator () (const SiStripCluster& cluster,
			const uint32_t& id) const
    { return signalOverNoise(cluster,id); }
   private:
    const std::string qualityLabel_;
    const SiStripQuality* quality_;
    const SiStripNoises* noise_;
    const SiStripGain* gain_;
    uint32_t qualityCacheId_;
    uint32_t noiseCacheId_;
    uint32_t gainCacheId_;
  };
  */
}

// ***********************************************************
JetAnalyzer::JetAnalyzer(const edm::ParameterSet& pSet)
  : trackPropagator_(new jetAnalysis::TrackPropagatorToCalo)//,
    //sOverNCalculator_(new jetAnalysis::StripSignalOverNoiseCalculator)
{
  parameters_ = pSet.getParameter<edm::ParameterSet>("jetAnalysis");
  outputMEsInRootFile   = pSet.getParameter<bool>("OutputMEsInRootFile");
  mInputCollection_           =    pSet.getParameter<edm::InputTag>       ("jetsrc");
  
  mOutputFile_   = pSet.getParameter<std::string>("OutputFile");
  jetType_ = pSet.getParameter<std::string>("JetType");
  jetCorrectionService_ = pSet.getParameter<std::string> ("JetCorrections");
  
  isCaloJet_ = (std::string("calo")==jetType_);
  isJPTJet_  = (std::string("jpt") ==jetType_);
  isPFJet_   = (std::string("pf") ==jetType_);
  
  if (isCaloJet_)  caloJetsToken_ = consumes<reco::CaloJetCollection>(mInputCollection_);
  if (isJPTJet_)   jptJetsToken_ = consumes<reco::JPTJetCollection>(mInputCollection_);
  if (isPFJet_)    pfJetsToken_ = consumes<reco::PFJetCollection>(mInputCollection_);
  
  JetIDQuality_  = pSet.getParameter<string>("JetIDQuality");
  JetIDVersion_  = pSet.getParameter<string>("JetIDVersion");

  // JetID definitions for Calo and JPT Jets
  if(isJPTJet_ || isCaloJet_){
    inputJetIDValueMap      = pSet.getParameter<edm::InputTag>("InputJetIDValueMap");
    jetID_ValueMapToken_= consumes< edm::ValueMap<reco::JetID> >(inputJetIDValueMap);
    if(JetIDVersion_== "PURE09"){
      jetidversion = JetIDSelectionFunctor::PURE09;
    }else if (JetIDVersion_== "DQM09"){
      jetidversion = JetIDSelectionFunctor::DQM09;
    }else if (JetIDVersion_=="CRAFT08"){
      jetidversion = JetIDSelectionFunctor::CRAFT08;
    }else{
      std::cout<<"no Valid JetID version given"<<std::endl;
    }
    if(JetIDQuality_== "MINIMAL"){
      jetidquality = JetIDSelectionFunctor::MINIMAL;
    }else if (JetIDQuality_== "LOOSE_AOD"){
      jetidquality = JetIDSelectionFunctor::LOOSE_AOD;
    }else if (JetIDQuality_=="LOOSE"){
      jetidquality = JetIDSelectionFunctor::LOOSE;
    }else if (JetIDQuality_=="TIGHT"){
      jetidquality = JetIDSelectionFunctor::TIGHT;
    }else{
      std::cout<<"no Valid JetID quality given"<<std::endl;
    }
    jetIDFunctor=JetIDSelectionFunctor( jetidversion, jetidquality);

  }

  //Jet ID definitions for PFJets
  if(isPFJet_){
    if(JetIDVersion_== "FIRSTDATA"){
      pfjetidversion = PFJetIDSelectionFunctor::FIRSTDATA;
    }else{
      std::cout<<"no valid PF JetID version given"<<std::endl;
    }
    if (JetIDQuality_=="LOOSE"){
      pfjetidquality = PFJetIDSelectionFunctor::LOOSE;
    }else if (JetIDQuality_=="TIGHT"){
      pfjetidquality = PFJetIDSelectionFunctor::TIGHT;
    }else{
      std::cout<<"no Valid PFJetID quality given"<<std::endl;
    }
    pfjetIDFunctor=PFJetIDSelectionFunctor( pfjetidversion, pfjetidquality);
  }
  //check later if some of those are also needed for PFJets
  leadJetFlag_ = 0;
  jetLoPass_   = 0;
  jetHiPass_   = 0;
  ptThreshold_ = 20.;
  ptThresholdUnc_ = 20.;
  asymmetryThirdJetCut_ = 5.;
  balanceThirdJetCut_   = 0.2; 

  theTriggerResultsLabel_        = pSet.getParameter<edm::InputTag>("TriggerResultsLabel");
  triggerResultsToken_          = consumes<edm::TriggerResults>(edm::InputTag(theTriggerResultsLabel_));
  //
  diJetSelectionFlag_         = pSet.getUntrackedParameter<bool>("DoDiJetSelection", true);
  jetCleaningFlag_            = pSet.getUntrackedParameter<bool>("JetCleaningFlag", true);
 
  // ==========================================================
  //DCS information
  // ==========================================================
  DCSFilterForJetMonitoring_  = new JetMETDQMDCSFilter(pSet.getParameter<ParameterSet>("DCSFilterForJetMonitoring"));
  DCSFilterForDCSMonitoring_  = new JetMETDQMDCSFilter("ecal:hbhe:hf:ho:pixel:sistrip:es:muon");
  
  //Trigger selectoin
  edm::ParameterSet highptjetparms = pSet.getParameter<edm::ParameterSet>("highPtJetTrigger");
  edm::ParameterSet lowptjetparms  = pSet.getParameter<edm::ParameterSet>("lowPtJetTrigger" );
  
  highPtJetEventFlag_ = new GenericTriggerEventFlag( highptjetparms, consumesCollector() );
  lowPtJetEventFlag_  = new GenericTriggerEventFlag( lowptjetparms , consumesCollector() );
  
  highPtJetExpr_ = highptjetparms.getParameter<std::vector<std::string> >("hltPaths");
  lowPtJetExpr_  = lowptjetparms .getParameter<std::vector<std::string> >("hltPaths");
  
  LSBegin_     = pSet.getParameter<int>("LSBegin");
  LSEnd_       = pSet.getParameter<int>("LSEnd");
  
  processname_ = pSet.getParameter<std::string>("processname");
  
  //jet cleanup parameters
  cleaningParameters_ = pSet.getParameter<ParameterSet>("CleaningParameters");

  bypassAllPVChecks_= cleaningParameters_.getParameter<bool>("bypassAllPVChecks");
  vertexLabel_      = cleaningParameters_.getParameter<edm::InputTag>("vertexCollection");
  vertexToken_      = consumes<std::vector<reco::Vertex> >(edm::InputTag(vertexLabel_));

  gtLabel_          = cleaningParameters_.getParameter<edm::InputTag>("gtLabel");
  gtToken_          = consumes<L1GlobalTriggerReadoutRecord>(edm::InputTag(gtLabel_));
  
  std::string inputCollectionLabel(mInputCollection_.label());
  
}  
  

// ***********************************************************
JetAnalyzer::~JetAnalyzer() {
  
  delete highPtJetEventFlag_;
  delete lowPtJetEventFlag_;

  delete DCSFilterForDCSMonitoring_;
  delete DCSFilterForJetMonitoring_;
}

// ***********************************************************
void JetAnalyzer::beginJob(void) {
  
  dbe_ = edm::Service<DQMStore>().operator->();
  if(diJetSelectionFlag_){
    dbe_->setCurrentFolder("JetMET/Jet/DiJets"+mInputCollection_.label());
  }else{
    if(jetCleaningFlag_){
      dbe_->setCurrentFolder("JetMET/Jet/Cleaned"+mInputCollection_.label());
      DirName = "JetMET/Jet/Cleaned"+mInputCollection_.label();
    }else{
      dbe_->setCurrentFolder("JetMET/Jet/Uncleaned"+mInputCollection_.label());
      DirName = "JetMET/Jet/Uncleaned"+mInputCollection_.label();
    }
  }
  fillJIDPassFrac_    = parameters_.getParameter<int>("fillJIDPassFrac");

  // monitoring of eta parameter
  
  verbose_= parameters_.getParameter<int>("verbose");

  etaBin_ = parameters_.getParameter<int>("etaBin");
  etaMin_ = parameters_.getParameter<double>("etaMin");
  etaMax_ = parameters_.getParameter<double>("etaMax");

  // monitoring of phi paramater
  phiBin_ = parameters_.getParameter<int>("phiBin");
  phiMin_ = parameters_.getParameter<double>("phiMin");
  phiMax_ = parameters_.getParameter<double>("phiMax");

  // monitoring of the transverse momentum
  ptBin_ = parameters_.getParameter<int>("ptBin");
  ptMin_ = parameters_.getParameter<double>("ptMin");
  ptMax_ = parameters_.getParameter<double>("ptMax");

  // 
  eBin_ = parameters_.getParameter<int>("eBin");
  eMin_ = parameters_.getParameter<double>("eMin");
  eMax_ = parameters_.getParameter<double>("eMax");

  // 
  pBin_ = parameters_.getParameter<int>("pBin");
  pMin_ = parameters_.getParameter<double>("pMin");
  pMax_ = parameters_.getParameter<double>("pMax");

  // 
  nbinsPV_ = parameters_.getParameter<int>("pVBin");
  nPVlow_   = parameters_.getParameter<double>("pVMin");
  nPVhigh_  = parameters_.getParameter<double>("pVMax");

  //
  ptThreshold_ = parameters_.getParameter<double>("ptThreshold");
  ptThresholdUnc_=parameters_.getParameter<double>("ptThresholdUnc");
  asymmetryThirdJetCut_ = parameters_.getParameter<double>("asymmetryThirdJetCut");
  balanceThirdJetCut_   = parameters_.getParameter<double>("balanceThirdJetCut");

  // Generic jet parameters
  mPt           = dbe_->book1D("Pt",           "pt",                 ptBin_,  ptMin_,  ptMax_);
  mEta          = dbe_->book1D("Eta",          "eta",               etaBin_, etaMin_, etaMax_);
  mPhi          = dbe_->book1D("Phi",          "phi",               phiBin_, phiMin_, phiMax_);
  if(!isJPTJet_){
    mConstituents = dbe_->book1D("Constituents", "# of constituents",     50,      0,    100);
  }
  mJetEnergyCorr= dbe_->book1D("JetEnergyCorr", "jet energy correction factor", 50, 0.0,3.0);
  mJetEnergyCorrVSEta= dbe_->bookProfile("JetEnergyCorrVSEta", "jet energy correction factor VS eta", etaBin_, etaMin_,etaMax_, 0.0,3.0);
  mJetEnergyCorrVSPt= dbe_->bookProfile("JetEnergyCorrVSPt", "jet energy correction factor VS pt", ptBin_, ptMin_,ptMax_, 0.0,3.0);
  mHFrac        = dbe_->book1D("HFrac",        "HFrac",                140,   -0.2,    1.2);
  mEFrac        = dbe_->book1D("EFrac",        "EFrac",                140,   -0.2,    1.2);

  if(!diJetSelectionFlag_ ){
    mPt_uncor           = dbe_->book1D("Pt_uncor",           "pt for uncorrected jets",                 ptBin_,  ptThresholdUnc_,  ptMax_);
    mEta_uncor          = dbe_->book1D("Eta_uncor",          "eta for uncorrected jets",               etaBin_, etaMin_, etaMax_);
    mPhi_uncor          = dbe_->book1D("Phi_uncor",          "phi for uncorrected jets",               phiBin_, phiMin_, phiMax_);
    if(!isJPTJet_){
      mConstituents_uncor = dbe_->book1D("Constituents_uncor", "# of constituents for uncorrected jets",     50,      0,    100);
    }
  }

  mDPhi                   = dbe_->book1D("DPhi", "dPhi btw the two leading jets", 100, 0., acos(-1.));
  
  mDijetAsymmetry                   = dbe_->book1D("DijetAsymmetry", "DijetAsymmetry", 100, -1., 1.);
  mDijetBalance                     = dbe_->book1D("DijetBalance",   "DijetBalance",   100, -2., 2.);

  // Book NPV profiles
  //----------------------------------------------------------------------------
  mPt_profile           = dbe_->bookProfile("Pt_profile",           "pt",                nbinsPV_, nPVlow_, nPVhigh_,   ptBin_,  ptMin_,  ptMax_);
  mEta_profile          = dbe_->bookProfile("Eta_profile",          "eta",               nbinsPV_, nPVlow_, nPVhigh_,  etaBin_, etaMin_, etaMax_);
  mPhi_profile          = dbe_->bookProfile("Phi_profile",          "phi",               nbinsPV_, nPVlow_, nPVhigh_,  phiBin_, phiMin_, phiMax_);
  if(!isJPTJet_){
    mConstituents_profile = dbe_->bookProfile("Constituents_profile", "# of constituents", nbinsPV_, nPVlow_, nPVhigh_,      50,      0,    100);
  }
  mHFrac_profile        = dbe_->bookProfile("HFrac_profile",        "HFrac",             nbinsPV_, nPVlow_, nPVhigh_,     140,   -0.2,    1.2);
  mEFrac_profile        = dbe_->bookProfile("EFrac_profile",        "EFrac",             nbinsPV_, nPVlow_, nPVhigh_,     140,   -0.2,    1.2);

  if(!jetCleaningFlag_){//fillJIDPassFrac_ defines a collection of cleaned jets, for which we will want to fill the cleaning passing fraction
    mLooseJIDPassFractionVSeta      = dbe_->bookProfile("JetIDPassFractionVSeta","JetIDPassFractionVSeta",etaBin_, etaMin_, etaMax_,0.,1.2);
    mLooseJIDPassFractionVSpt       = dbe_->bookProfile("JetIDPassFractionVSpt","JetIDPassFractionVSpt",ptBin_, ptMin_, ptMax_,0.,1.2);
    mLooseJIDPassFractionVSptNoHF   = dbe_->bookProfile("JetIDPassFractionVSptNoHF","JetIDPassFractionVSptNoHF",ptBin_, ptMin_, ptMax_,0.,1.2);
  }

  if (!diJetSelectionFlag_ ){
    mNJets_profile = dbe_->bookProfile("NJets_profile", "number of jets", nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 100);
  }

  // Set NPV profiles x-axis title
  //----------------------------------------------------------------------------
  mPt_profile          ->setAxisTitle("nvtx",1);
  mEta_profile         ->setAxisTitle("nvtx",1);
  mPhi_profile         ->setAxisTitle("nvtx",1);
  if(!isJPTJet_){
    mConstituents_profile->setAxisTitle("nvtx",1);
  }
  mHFrac_profile       ->setAxisTitle("nvtx",1);
  mEFrac_profile       ->setAxisTitle("nvtx",1);

  if (!diJetSelectionFlag_ ) {
    mNJets_profile->setAxisTitle("nvtx",1);
  }


  //mE                       = dbe_->book1D("E", "E", eBin_, eMin_, eMax_);
  //mP                       = dbe_->book1D("P", "P", pBin_, pMin_, pMax_);
  //  mMass                    = dbe_->book1D("Mass", "Mass", 100, 0, 25);
  //
  mPhiVSEta                     = dbe_->book2D("PhiVSEta", "PhiVSEta", 50, etaMin_, etaMax_, 24, phiMin_, phiMax_);
  mPhiVSEta->getTH2F()->SetOption("colz");
  mPhiVSEta->setAxisTitle("#eta",1);
  mPhiVSEta->setAxisTitle("#phi",2);
  if(!diJetSelectionFlag_ ){
    mPt_1                    = dbe_->book1D("Pt_1", "Pt spectrum of jets - range 1", 20, 0, 100);   
    mPt_2                    = dbe_->book1D("Pt_2", "Pt spectrum of jets - range 2", 60, 0, 300);   
    mPt_3                    = dbe_->book1D("Pt_3", "Pt spectrum of jets - range 3", 100, 0, 5000);
    // Low and high pt trigger paths
    mPt_Lo                  = dbe_->book1D("Pt_Lo", "Pt (Pass Low Pt Jet Trigger)", 20, 0, 100);   
    //mEta_Lo                 = dbe_->book1D("Eta_Lo", "Eta (Pass Low Pt Jet Trigger)", etaBin_, etaMin_, etaMax_);
    mPhi_Lo                 = dbe_->book1D("Phi_Lo", "Phi (Pass Low Pt Jet Trigger)", phiBin_, phiMin_, phiMax_);
    
    mPt_Hi                  = dbe_->book1D("Pt_Hi", "Pt (Pass Hi Pt Jet Trigger)", 60, 0, 300);   
    mEta_Hi                 = dbe_->book1D("Eta_Hi", "Eta (Pass Hi Pt Jet Trigger)", etaBin_, etaMin_, etaMax_);
    mPhi_Hi                 = dbe_->book1D("Phi_Hi", "Phi (Pass Hi Pt Jet Trigger)", phiBin_, phiMin_, phiMax_);
    mNJets                   = dbe_->book1D("NJets", "number of jets", 100, 0, 100);

    //mPt_Barrel_Lo            = dbe_->book1D("Pt_Barrel_Lo", "Pt Barrel (Pass Low Pt Jet Trigger)", 20, 0, 100);   
    //mPhi_Barrel_Lo           = dbe_->book1D("Phi_Barrel_Lo", "Phi Barrel (Pass Low Pt Jet Trigger)", phiBin_, phiMin_, phiMax_);
    if(!isJPTJet_){
      mConstituents_Barrel     = dbe_->book1D("Constituents_Barrel", "Constituents Barrel", 50, 0, 100);
    }
    mHFrac_Barrel            = dbe_->book1D("HFrac_Barrel", "HFrac Barrel", 100, 0, 1);
    mEFrac_Barrel            = dbe_->book1D("EFrac_Barrel", "EFrac Barrel", 110, -0.05, 1.05);
    
    //mPt_EndCap_Lo            = dbe_->book1D("Pt_EndCap_Lo", "Pt EndCap (Pass Low Pt Jet Trigger)", 20, 0, 100);   
    //mPhi_EndCap_Lo           = dbe_->book1D("Phi_EndCap_Lo", "Phi EndCap (Pass Low Pt Jet Trigger)", phiBin_, phiMin_, phiMax_);
    if(!isJPTJet_){
      mConstituents_EndCap     = dbe_->book1D("Constituents_EndCap", "Constituents EndCap", 50, 0, 100);
    }
    mHFrac_EndCap            = dbe_->book1D("HFrac_EndCap", "HFrac EndCap", 100, 0, 1);
    mEFrac_EndCap            = dbe_->book1D("EFrac_EndCap", "EFrac EndCap", 110, -0.05, 1.05);
    
    //mPt_Forward_Lo           = dbe_->book1D("Pt_Forward_Lo", "Pt Forward (Pass Low Pt Jet Trigger)", 20, 0, 100);  
    //mPhi_Forward_Lo          = dbe_->book1D("Phi_Forward_Lo", "Phi Forward (Pass Low Pt Jet Trigger)", phiBin_, phiMin_, phiMax_);
    if(!isJPTJet_){
      mConstituents_Forward    = dbe_->book1D("Constituents_Forward", "Constituents Forward", 50, 0, 100);
    }
    mHFrac_Forward           = dbe_->book1D("HFrac_Forward", "HFrac Forward", 140, -0.2, 1.2);
    mEFrac_Forward           = dbe_->book1D("EFrac_Forward", "EFrac Forward", 140, -0.2, 1.2);
    
    mPt_Barrel_Hi            = dbe_->book1D("Pt_Barrel_Hi", "Pt Barrel (Pass Hi Pt Jet Trigger)", 60, 0, 300);   
    mPhi_Barrel_Hi           = dbe_->book1D("Phi_Barrel_Hi", "Phi Barrel (Pass Hi Pt Jet Trigger)", phiBin_, phiMin_, phiMax_);
    //mConstituents_Barrel_Hi  = dbe_->book1D("Constituents_Barrel_Hi", "Constituents Barrel (Pass Hi Pt Jet Trigger)", 50, 0, 100);
    //mHFrac_Barrel_Hi         = dbe_->book1D("HFrac_Barrel_Hi", "HFrac Barrel (Pass Hi Pt Jet Trigger)", 100, 0, 1);
    
    mPt_EndCap_Hi            = dbe_->book1D("Pt_EndCap_Hi", "Pt EndCap (Pass Hi Pt Jet Trigger)", 60, 0, 300);  
    mPhi_EndCap_Hi           = dbe_->book1D("Phi_EndCap_Hi", "Phi EndCap (Pass Hi Pt Jet Trigger)", phiBin_, phiMin_, phiMax_);
    //mConstituents_EndCap_Hi  = dbe_->book1D("Constituents_EndCap_Hi", "Constituents EndCap (Pass Hi Pt Jet Trigger)", 50, 0, 100);
    //mHFrac_EndCap_Hi         = dbe_->book1D("HFrac_EndCap_Hi", "HFrac EndCap (Pass Hi Pt Jet Trigger)", 100, 0, 1);
    
    mPt_Forward_Hi           = dbe_->book1D("Pt_Forward_Hi", "Pt Forward (Pass Hi Pt Jet Trigger)", 60, 0, 300);  
    mPhi_Forward_Hi          = dbe_->book1D("Phi_Forward_Hi", "Phi Forward (Pass Hi Pt Jet Trigger)", phiBin_, phiMin_, phiMax_);
    //mConstituents_Forward_Hi = dbe_->book1D("Constituents_Forward_Hi", "Constituents Forward (Pass Hi Pt Jet Trigger)", 50, 0, 100);
    //mHFrac_Forward_Hi        = dbe_->book1D("HFrac_Forward_Hi", "HFrac Forward (Pass Hi Pt Jet Trigger)", 100, 0, 1);
    
    mPhi_Barrel              = dbe_->book1D("Phi_Barrel", "Phi_Barrel", phiBin_, phiMin_, phiMax_);
    //mE_Barrel                = dbe_->book1D("E_Barrel", "E_Barrel", eBin_, eMin_, eMax_);
    mPt_Barrel               = dbe_->book1D("Pt_Barrel", "Pt_Barrel", ptBin_, ptMin_, ptMax_);
    
    mPhi_EndCap              = dbe_->book1D("Phi_EndCap", "Phi_EndCap", phiBin_, phiMin_, phiMax_);
    //mE_EndCap                = dbe_->book1D("E_EndCap", "E_EndCap", eBin_, eMin_, 2*eMax_);
    mPt_EndCap               = dbe_->book1D("Pt_EndCap", "Pt_EndCap", ptBin_, ptMin_, ptMax_);
    
    mPhi_Forward             = dbe_->book1D("Phi_Forward", "Phi_Forward", phiBin_, phiMin_, phiMax_);
    //mE_Forward               = dbe_->book1D("E_Forward", "E_Forward", eBin_, eMin_, 4*eMax_);
    mPt_Forward              = dbe_->book1D("Pt_Forward", "Pt_Forward", ptBin_, ptMin_, ptMax_);
    
    // Leading Jet Parameters
    mEtaFirst                = dbe_->book1D("EtaFirst", "EtaFirst", 100, -5, 5);
    mPhiFirst                = dbe_->book1D("PhiFirst", "PhiFirst", 70, -3.5, 3.5);
    //mEFirst                  = dbe_->book1D("EFirst", "EFirst", 100, 0, 1000);
    mPtFirst                 = dbe_->book1D("PtFirst", "PtFirst", ptBin_, ptMin_, ptMax_);

    //if (fillJIDPassFrac_==1) {
    //mLooseJIDPassFractionVSeta  = dbe_->bookProfile("LooseJIDPassFractionVSeta","LooseJIDPassFractionVSeta",50, -3., 3.,0.,1.2);
    //mLooseJIDPassFractionVSpt   = dbe_->bookProfile("LooseJIDPassFractionVSpt","LooseJIDPassFractionVSpt",ptBin_, ptMin_, ptMax_,0.,1.2);
    //mTightJIDPassFractionVSeta  = dbe_->bookProfile("TightJIDPassFractionVSeta","TightJIDPassFractionVSeta",50, -3., 3.,0.,1.2);
    //mTightJIDPassFractionVSpt   = dbe_->bookProfile("TightJIDPassFractionVSpt","TightJIDPassFractionVSpt",ptBin_, ptMin_, ptMax_,0.,1.2);
    //}
  }
  //
  //--- Calo jet selection only
  if(isCaloJet_) {
    jetME = dbe_->book1D("jetReco", "jetReco", 3, 1, 4);
    jetME->setBinLabel(1,"CaloJets",1);

    // CaloJet specific
    mMaxEInEmTowers         = dbe_->book1D("MaxEInEmTowers", "MaxEInEmTowers", 100, 0, 100);
    mMaxEInHadTowers        = dbe_->book1D("MaxEInHadTowers", "MaxEInHadTowers", 100, 0, 100);
    if(!diJetSelectionFlag_ ) {
      mHadEnergyInHO          = dbe_->book1D("HadEnergyInHO", "HadEnergyInHO", 100, 0, 10);
      mHadEnergyInHB          = dbe_->book1D("HadEnergyInHB", "HadEnergyInHB", 100, 0, 50);
      mHadEnergyInHF          = dbe_->book1D("HadEnergyInHF", "HadEnergyInHF", 100, 0, 50);
      mHadEnergyInHE          = dbe_->book1D("HadEnergyInHE", "HadEnergyInHE", 100, 0, 100);
      mEmEnergyInEB           = dbe_->book1D("EmEnergyInEB", "EmEnergyInEB", 100, 0, 50);
      mEmEnergyInEE           = dbe_->book1D("EmEnergyInEE", "EmEnergyInEE", 100, 0, 50);
      mEmEnergyInHF           = dbe_->book1D("EmEnergyInHF", "EmEnergyInHF", 120, -20, 100);
    }
    //JetID variables
    mresEMF                 = dbe_->book1D("resEMF", "resEMF", 50, 0., 1.);
    mN90Hits                = dbe_->book1D("N90Hits", "N90Hits", 50, 0., 50);
    mfHPD                   = dbe_->book1D("fHPD", "fHPD", 50, 0., 1.);
    mfRBX                   = dbe_->book1D("fRBX", "fRBX", 50, 0., 1.);
  }


  if(isJPTJet_) {
   jetME = dbe_->book1D("jetReco", "jetReco", 3, 1, 4);
   jetME->setBinLabel(3,"JPTJets",1);
   if(!diJetSelectionFlag_ ){
     //jpt histograms
     mE   = dbe_->book1D("E", "E", eBin_, eMin_, eMax_);
     mEt  = dbe_->book1D("Et", "Et", ptBin_, ptMin_, ptMax_);
     mP   = dbe_->book1D("P", "P", ptBin_, ptMin_, ptMax_);
     mPtSecond = dbe_->book1D("PtSecond", "PtSecond", ptBin_, ptMin_, ptMax_);
     mPtThird = dbe_->book1D("PtThird", "PtThird", ptBin_, ptMin_, ptMax_);
     mPx  = dbe_->book1D("Px", "Px", ptBin_, 0., ptMax_);
     mPy  = dbe_->book1D("Py", "Py", ptBin_, 0., ptMax_);
     mPz  = dbe_->book1D("Pz", "Pz", ptBin_, 0., ptMax_);

     //JetID variables
     
     mresEMF                 = dbe_->book1D("resEMF", "resEMF", 50, 0., 1.);
     mN90Hits                = dbe_->book1D("N90Hits", "N90Hits", 50, 0., 50);
     mfHPD                   = dbe_->book1D("fHPD", "fHPD", 50, 0., 1.);
     mfRBX                   = dbe_->book1D("fRBX", "fRBX", 50, 0., 1.);
     
     mnTracks  = dbe_->book1D("nTracks", "number of tracks for correction per jet", 100, 0, 100);
     mnTracksVSJetPt= dbe_->bookProfile("nTracksVSJetPt","number of tracks for correction per jet vs raw jet p_{T}",ptBin_, ptMin_, ptMax_,100,0,100);
     mnTracksVSJetEta= dbe_->bookProfile("nTracksVSJetEta","number of tracks for correction per jet vs jet #eta",etaBin_, etaMin_, etaMax_,100,0,100);
     mnTracksVSJetPt ->setAxisTitle("raw JetPt",1);
     mnTracksVSJetEta ->setAxisTitle("raw JetEta",1);
     //define meaningful limits

     //ntrackbins,0,ntrackbins
     mnallPionTracksPerJet=dbe_->book1D("nallPionTracks", "number of pion tracks for correction per jet", 100, 0, 100);
     //trackptbins,0,trackptmax
     //introduce etamax?
     mallPionTracksPt=dbe_->book1D("allPionTracksPt", "pion track p_{T}", 100, 0., 50.);
     mallPionTracksEta=dbe_->book1D("allPionTracksEta", "pion track #eta", 50, -2.5, 2.5);
     //phibins,phimax,phimin
     mallPionTracksPhi=dbe_->book1D("allPionTracksPhi", "pion track #phi", phiBin_,phiMin_, phiMax_);
     //etabins/etamax/etamin
     mallPionTracksPtVSEta=dbe_->bookProfile("allPionTracksPtVSEta", "pion track p_{T} vs track #eta", 50, -2.5, 2.5,100,0.,50.);

     mnInVertexInCaloPionTracksPerJet=dbe_->book1D("nInVertexInCaloPionTracks", "number of pion in cone at calo and vertexs for correction per jet", 100, 0, 100);
     mInVertexInCaloPionTracksPt=dbe_->book1D("InVertexInCaloPionTracksPt", "pion in cone at calo and vertex p_{T}", 100, 0., 50.);
     mInVertexInCaloPionTracksEta=dbe_->book1D("InVertexInCaloPionTracksEta", "pion in cone at calo and vertex #eta", 50, -2.5, 2.5);
     mInVertexInCaloPionTracksPhi=dbe_->book1D("InVertexInCaloPionTracksPhi", "pion in cone at calo and vertex #phi", phiBin_,phiMin_, phiMax_);
     mInVertexInCaloPionTracksPtVSEta=dbe_->bookProfile("InVertexInCaloPionTracksPtVSEta", "pion in cone at calo and vertex p_{T} vs #eta", 50, -2.5, 2.5,100,0.,50.);
     //monitor element trackDirectionJetDRHisto
     //monitor element trackImpactPointJetDRHisto
     mnOutVertexInCaloPionTracksPerJet=dbe_->book1D("nOutVertexInCaloPionTracks", "number of pion in cone at calo and out at vertex for correction per jet", 100, 0, 100);
     mOutVertexInCaloPionTracksPt=dbe_->book1D("OutVertexInCaloPionTracksPt", "pion in cone at calo and out at vertex p_{T}", 100, 0., 50.);
     mOutVertexInCaloPionTracksEta=dbe_->book1D("OutVertexInCaloPionTracksEta", "pion in cone at calo and out at vertex #eta", 50, -2.5, 2.5);
     mOutVertexInCaloPionTracksPhi=dbe_->book1D("OutVertexInCaloPionTracksPhi", "pion in cone at calo and out at vertex #phi", phiBin_,phiMin_, phiMax_);
     mOutVertexInCaloPionTracksPtVSEta=dbe_->bookProfile("OutVertexInCaloPionTracksPtVSEta", "pion in cone at calo and out at vertex p_{T} vs #eta", 50, -2.5, 2.5,100,0.,50.);

     mnInVertexOutCaloPionTracksPerJet=dbe_->book1D("nInVertexOutCaloPionTracks", "number of pions out cone at calo and and in cone at vertex for correction per jet", 100, 0, 100);
     mInVertexOutCaloPionTracksPt=dbe_->book1D("InVertexOutCaloPionTracksPt", "pion out cone at calo and in cone at vertex p_{T}", 100, 0., 50.);
     mInVertexOutCaloPionTracksEta=dbe_->book1D("InVertexOutCaloPionTracksEta", "pion out cone at calo and in cone at vertex #eta", 50, -2.5, 2.5);
     mInVertexOutCaloPionTracksPhi=dbe_->book1D("InVertexOutCaloPionTracksPhi", "pion out cone at calo and in cone at vertex #phi", phiBin_,phiMin_, phiMax_);
     mInVertexOutCaloPionTracksPtVSEta=dbe_->bookProfile("InVertexOutCaloPionTracksPtVSEta", "pion out cone at calo and in cone at vertex p_{T} vs #eta", 50, -2.5, 2.5,100,0.,50.);

     mnallMuonTracksPerJet=dbe_->book1D("nallMuonTracks", "number of muon tracks for correction per jet", 10, 0, 10);
     mallMuonTracksPt=dbe_->book1D("allMuonTracksPt", "muon track p_{T}", 100, 0., 50.);
     mallMuonTracksEta=dbe_->book1D("allMuonTrackEta", "muon track #eta", 50, -2.5, 2.5);
     mallMuonTracksPhi=dbe_->book1D("allMuonTracksPhi", "muon track #phi", phiBin_,phiMin_, phiMax_);
     mallMuonTracksPtVSEta=dbe_->bookProfile("allMuonTracksPtVSEta", "muon track p_{T} vs track #eta", 50, -2.5, 2.5,100,0.,50.);
 
     mnInVertexInCaloMuonTracksPerJet=dbe_->book1D("nInVertexInCaloMuonTracks", "number of muons in cone at calo and vertex for correction per jet", 10, 0, 10);
     mInVertexInCaloMuonTracksPt=dbe_->book1D("InVertexInCaloMuonTracksPt", "muon in cone at calo and vertex p_{T}", 100, 0., 50.);
     mInVertexInCaloMuonTracksEta=dbe_->book1D("InVertexInCaloMuonTracksEta", "muon in cone at calo and vertex #eta", 50, -2.5, 2.5);
     mInVertexInCaloMuonTracksPhi=dbe_->book1D("InVertexInCaloMuonTracksPhi", "muon in cone at calo and vertex #phi", phiBin_,phiMin_, phiMax_);
     mInVertexInCaloMuonTracksPtVSEta=dbe_->bookProfile("InVertexInCaloMuonTracksPtVSEta", "muon in cone at calo and vertex p_{T} vs #eta", 50, -2.5, 2.5,100,0.,50.);

     mnOutVertexInCaloMuonTracksPerJet=dbe_->book1D("nOutVertexInCaloMuonTracks", "number of muons in cone at calo and out cone at vertex for correction per jet", 10, 0, 10);
     mOutVertexInCaloMuonTracksPt=dbe_->book1D("OutVertexInCaloMuonTracksPt", "muon in cone at calo and out cone at vertex p_{T}", 100, 0., 50.);
     mOutVertexInCaloMuonTracksEta=dbe_->book1D("OutVertexInCaloMuonTracksEta", "muon in cone at calo and out cone at vertex #eta", 50, -2.5, 2.5);
     mOutVertexInCaloMuonTracksPhi=dbe_->book1D("OutVertexInCaloMuonTracksPhi", "muon in cone at calo and out cone at vertex #phi", phiBin_,phiMin_, phiMax_);
     mOutVertexInCaloMuonTracksPtVSEta=dbe_->bookProfile("OutVertexInCaloMuonTracksPtVSEta", "muon oin cone at calo and out cone at vertex p_{T} vs #eta", 50, -2.5, 2.5,100,0.,50.);

     mnInVertexOutCaloMuonTracksPerJet=dbe_->book1D("nInVertexOutCaloMuonTracks", "number of muons out cone at calo and in cone at vertex for correction per jet", 10, 0, 10);
     mInVertexOutCaloMuonTracksPt=dbe_->book1D("InVertexOutCaloMuonTracksPt", "muon out cone at calo and in cone at vertex p_{T}", 100, 0., 50.);
     mInVertexOutCaloMuonTracksEta=dbe_->book1D("InVertexOutCaloMuonTracksEta", "muon out cone at calo and in cone at vertex #eta", 50, -2.5, 2.5);
     mInVertexOutCaloMuonTracksPhi=dbe_->book1D("InVertexOutCaloMuonTracksPhi", "muon out cone at calo and in cone at vertex #phi", phiBin_,phiMin_, phiMax_);
     mInVertexOutCaloMuonTracksPtVSEta=dbe_->bookProfile("InVertexOutCaloMuonTracksPtVSEta", "muon out cone at calo and in cone at vertex p_{T} vs #eta", 50, -2.5, 2.5,100,0.,50.);

     mnallElectronTracksPerJet=dbe_->book1D("nallElectronTracks", "number of electron tracks for correction per jet", 10, 0, 10);
     mallElectronTracksPt=dbe_->book1D("allElectronTracksPt", "electron track p_{T}", 100, 0., 50.);
     mallElectronTracksEta=dbe_->book1D("allElectronTracksEta", "electron track #eta", 50, -2.5, 2.5);
     mallElectronTracksPhi=dbe_->book1D("allElectronTracksPhi", "electron track #phi", phiBin_,phiMin_, phiMax_);
     mallElectronTracksPtVSEta=dbe_->bookProfile("allElectronTracksPtVSEta", "electron track p_{T} vs track #eta", 50, -2.5, 2.5,100,0.,50.);

     mnInVertexInCaloElectronTracksPerJet=dbe_->book1D("nInVertexInCaloElectronTracks", "number of electrons in cone at calo and vertex for correction per jet", 10, 0, 10);
     mInVertexInCaloElectronTracksPt=dbe_->book1D("InVertexInCaloElectronTracksPt", "electron in cone at calo and vertex p_{T}", 100, 0., 50.);
     mInVertexInCaloElectronTracksEta=dbe_->book1D("InVertexInCaloElectronTracksEta", "electron in cone at calo and vertex #eta", 50, -2.5, 2.5);
     mInVertexInCaloElectronTracksPhi=dbe_->book1D("InVertexInCaloElectronTracksPhi", "electron in cone at calo and vertex #phi", phiBin_,phiMin_, phiMax_);
     mInVertexInCaloElectronTracksPtVSEta=dbe_->bookProfile("InVertexInCaloElectronTracksPtVSEta", "electron in cone at calo and vertex  p_{T} vs #eta", 50, -2.5, 2.5,100,0.,50.);

     mnOutVertexInCaloElectronTracksPerJet=dbe_->book1D("nOutVertexInCaloElectronTracks", "number of electrons in cone at calo and out cone at vertex for correction per jet", 10, 0, 10);
     mOutVertexInCaloElectronTracksPt=dbe_->book1D("OutVertexInCaloElectronTracksPt", "electron in cone at calo and out cone at vertex p_{T}", 100, 0., 50.);
     mOutVertexInCaloElectronTracksEta=dbe_->book1D("OutVertexInCaloElectronTracksEta", "electron in cone at calo and out cone at vertex #eta", 50, -2.5, 2.5);
     mOutVertexInCaloElectronTracksPhi=dbe_->book1D("OutVertexInCaloElectronTracksPhi", "electron in cone at calo and out cone at vertex #phi", phiBin_,phiMin_, phiMax_);
     mOutVertexInCaloElectronTracksPtVSEta=dbe_->bookProfile("OutVertexInCaloElectronTracksPtVSEta", "electron in cone at calo and out cone at vertex p_{T} vs #eta", 50, -2.5, 2.5,100,0.,50.);

     mnInVertexOutCaloElectronTracksPerJet=dbe_->book1D("nInVertexOutCaloElectronTracks", "number of electrons out cone at calo and in cone at vertex for correction per jet", 10, 0, 10);
     mInVertexOutCaloElectronTracksPt=dbe_->book1D("InVertexOutCaloElectronTracksPt", "electron out cone at calo and in cone at vertex p_{T}", 100, 0., 50.);
     mInVertexOutCaloElectronTracksEta=dbe_->book1D("InVertexOutCaloElectronTracksEta", "electron out cone at calo and in cone at vertex #eta", 50, -2.5, 2.5);
     mInVertexOutCaloElectronTracksPhi=dbe_->book1D("InVertexOutCaloElectronTracksPhi", "electron out cone at calo and in cone at vertex #phi", phiBin_,phiMin_, phiMax_);
     mInVertexOutCaloElectronTracksPtVSEta=dbe_->bookProfile("InVertexOutCaloElectronTracksPtVSEta", "electron out cone at calo and in cone at vertex p_{T} vs #eta", 50, -2.5, 2.5,100,0.,50.);

     mInCaloTrackDirectionJetDRHisto_      = dbe_->book1D("InCaloTrackDirectionJetDR",
							"#Delta R between track direction at vertex and jet axis (track in cone at calo)",50,0.,2.0);
     mOutCaloTrackDirectionJetDRHisto_      = dbe_->book1D("OutCaloTrackDirectionJetDR",
							"#Delta R between track direction at vertex and jet axis (track out cone at calo)",50,0.,2.0);
     mInVertexTrackImpactPointJetDRHisto_  = dbe_->book1D("InVertexTrackImpactPointJetDR",
							 "#Delta R between track impact point on calo and jet axis (track in cone at vertex)",50,0.,2.0);
     mOutVertexTrackImpactPointJetDRHisto_ = dbe_->book1D("OutVertexTrackImpactPointJetDR",
							 "#Delta R between track impact point on calo and jet axis (track out of cone at vertex)",50,0.,2.0);
   }
  }

  if(isPFJet_) {
    jetME = dbe_->book1D("jetReco", "jetReco", 3, 1, 4);
    jetME->setBinLabel(2,"PFJets",1);

    if(!diJetSelectionFlag_ ){
      //PFJet specific
      mCHFracVSeta_lowPt= dbe_->bookProfile("CHFracVSeta_lowPt","CHFracVSeta_lowPt",etaBin_, etaMin_, etaMax_,0.,1.2);
      mNHFracVSeta_lowPt= dbe_->bookProfile("NHFracVSeta_lowPt","NHFracVSeta_lowPt",etaBin_, etaMin_, etaMax_,0.,1.2);
      mPhFracVSeta_lowPt= dbe_->bookProfile("PhFracVSeta_lowPt","PhFracVSeta_lowPt",etaBin_, etaMin_, etaMax_,0.,1.2);
      mElFracVSeta_lowPt= dbe_->bookProfile("ElFracVSeta_lowPt","ElFracVSeta_lowPt",etaBin_, etaMin_, etaMax_,0.,1.2);
      mMuFracVSeta_lowPt= dbe_->bookProfile("MuFracVSeta_lowPt","MuFracVSeta_lowPt",etaBin_, etaMin_, etaMax_,0.,1.2);
      mCHFracVSeta_mediumPt= dbe_->bookProfile("CHFracVSeta_mediumPt","CHFracVSeta_mediumPt",etaBin_, etaMin_, etaMax_,0.,1.2);
      mNHFracVSeta_mediumPt= dbe_->bookProfile("NHFracVSeta_mediumPt","NHFracVSeta_mediumPt",etaBin_, etaMin_, etaMax_,0.,1.2);
      mPhFracVSeta_mediumPt= dbe_->bookProfile("PhFracVSeta_mediumPt","PhFracVSeta_mediumPt",etaBin_, etaMin_, etaMax_,0.,1.2);
      mElFracVSeta_mediumPt= dbe_->bookProfile("ElFracVSeta_mediumPt","ElFracVSeta_mediumPt",etaBin_, etaMin_, etaMax_,0.,1.2);
      mMuFracVSeta_mediumPt= dbe_->bookProfile("MuFracVSeta_mediumPt","MuFracVSeta_mediumPt",etaBin_, etaMin_, etaMax_,0.,1.2);
      mCHFracVSeta_highPt= dbe_->bookProfile("CHFracVSeta_highPt","CHFracVSeta_highPt",etaBin_, etaMin_, etaMax_,0.,1.2);
      mNHFracVSeta_highPt= dbe_->bookProfile("NHFracVSeta_highPt","NHFracVSeta_highPt",etaBin_, etaMin_, etaMax_,0.,1.2);
      mPhFracVSeta_highPt= dbe_->bookProfile("PhFracVSeta_highPt","PhFracVSeta_highPt",etaBin_, etaMin_, etaMax_,0.,1.2);
      mElFracVSeta_highPt= dbe_->bookProfile("ElFracVSeta_highPt","ElFracVSeta_highPt",etaBin_, etaMin_, etaMax_,0.,1.2);
      mMuFracVSeta_highPt= dbe_->bookProfile("MuFracVSeta_highPt","MuFracVSeta_highPt",etaBin_, etaMin_, etaMax_,0.,1.2);
      
      //barrel histograms for PFJets
      // energy fractions
      mCHFrac_lowPt_Barrel     = dbe_->book1D("CHFrac_lowPt_Barrel", "CHFrac_lowPt_Barrel", 120, -0.1, 1.1);
      mNHFrac_lowPt_Barrel     = dbe_->book1D("NHFrac_lowPt_Barrel", "NHFrac_lowPt_Barrel", 120, -0.1, 1.1);
      mPhFrac_lowPt_Barrel     = dbe_->book1D("PhFrac_lowPt_Barrel", "PhFrac_lowPt_Barrel", 120, -0.1, 1.1);
      mElFrac_lowPt_Barrel     = dbe_->book1D("ElFrac_lowPt_Barrel", "ElFrac_lowPt_Barrel", 120, -0.1, 1.1);
      mMuFrac_lowPt_Barrel     = dbe_->book1D("MuFrac_lowPt_Barrel", "MuFrac_lowPt_Barrel", 120, -0.1, 1.1);
      mCHFrac_mediumPt_Barrel  = dbe_->book1D("CHFrac_mediumPt_Barrel", "CHFrac_mediumPt_Barrel", 120, -0.1, 1.1);
      mNHFrac_mediumPt_Barrel  = dbe_->book1D("NHFrac_mediumPt_Barrel", "NHFrac_mediumPt_Barrel", 120, -0.1, 1.1);
      mPhFrac_mediumPt_Barrel  = dbe_->book1D("PhFrac_mediumPt_Barrel", "PhFrac_mediumPt_Barrel", 120, -0.1, 1.1);
      mElFrac_mediumPt_Barrel  = dbe_->book1D("ElFrac_mediumPt_Barrel", "ElFrac_mediumPt_Barrel", 120, -0.1, 1.1);
      mMuFrac_mediumPt_Barrel  = dbe_->book1D("MuFrac_mediumPt_Barrel", "MuFrac_mediumPt_Barrel", 120, -0.1, 1.1);
      mCHFrac_highPt_Barrel    = dbe_->book1D("CHFrac_highPt_Barrel", "CHFrac_highPt_Barrel", 120, -0.1, 1.1);
      mNHFrac_highPt_Barrel    = dbe_->book1D("NHFrac_highPt_Barrel", "NHFrac_highPt_Barrel", 120, -0.1, 1.1);
      mPhFrac_highPt_Barrel    = dbe_->book1D("PhFrac_highPt_Barrel", "PhFrac_highPt_Barrel", 120, -0.1, 1.1);
      mElFrac_highPt_Barrel    = dbe_->book1D("ElFrac_highPt_Barrel", "ElFrac_highPt_Barrel", 120, -0.1, 1.1);
      mMuFrac_highPt_Barrel    = dbe_->book1D("MuFrac_highPt_Barrel", "MuFrac_highPt_Barrel", 120, -0.1, 1.1);
      
      //energies
      mCHEn_lowPt_Barrel     = dbe_->book1D("CHEn_lowPt_Barrel", "CHEn_lowPt_Barrel", ptBin_, 0., ptMax_);
      mNHEn_lowPt_Barrel     = dbe_->book1D("NHEn_lowPt_Barrel", "NHEn_lowPt_Barrel", ptBin_, 0., ptMax_);
      mPhEn_lowPt_Barrel     = dbe_->book1D("PhEn_lowPt_Barrel", "PhEn_lowPt_Barrel", ptBin_, 0., ptMax_);
      mElEn_lowPt_Barrel     = dbe_->book1D("ElEn_lowPt_Barrel", "ElEn_lowPt_Barrel", ptBin_, 0., ptMax_);
      mMuEn_lowPt_Barrel     = dbe_->book1D("MuEn_lowPt_Barrel", "MuEn_lowPt_Barrel", ptBin_, 0., ptMax_);
      mCHEn_mediumPt_Barrel  = dbe_->book1D("CHEn_mediumPt_Barrel", "CHEn_mediumPt_Barrel", ptBin_, 0., ptMax_);
      mNHEn_mediumPt_Barrel  = dbe_->book1D("NHEn_mediumPt_Barrel", "NHEn_mediumPt_Barrel", ptBin_, 0., ptMax_);
      mPhEn_mediumPt_Barrel  = dbe_->book1D("PhEn_mediumPt_Barrel", "PhEn_mediumPt_Barrel", ptBin_, 0., ptMax_);
      mElEn_mediumPt_Barrel  = dbe_->book1D("ElEn_mediumPt_Barrel", "ElEn_mediumPt_Barrel", ptBin_, 0., ptMax_);
      mMuEn_mediumPt_Barrel  = dbe_->book1D("MuEn_mediumPt_Barrel", "MuEn_mediumPt_Barrel", ptBin_, 0., ptMax_);
      mCHEn_highPt_Barrel    = dbe_->book1D("CHEn_highPt_Barrel", "CHEn_highPt_Barrel", ptBin_, 0., ptMax_);
      mNHEn_highPt_Barrel    = dbe_->book1D("NHEn_highPt_Barrel", "NHEn_highPt_Barrel", ptBin_, 0., ptMax_);
      mPhEn_highPt_Barrel    = dbe_->book1D("PhEn_highPt_Barrel", "PhEn_highPt_Barrel", ptBin_, 0., ptMax_);
      mElEn_highPt_Barrel    = dbe_->book1D("ElEn_highPt_Barrel", "ElEn_highPt_Barrel", ptBin_, 0., ptMax_);
      mMuEn_highPt_Barrel    = dbe_->book1D("MuEn_highPt_Barrel", "MuEn_highPt_Barrel", ptBin_, 0., ptMax_);
      //multiplicities
      mChMultiplicity_lowPt_Barrel    = dbe_->book1D("ChMultiplicity_lowPt_Barrel", "ChMultiplicity_lowPt_Barrel", 30,0,30);
      mNeutMultiplicity_lowPt_Barrel   = dbe_->book1D("NeutMultiplicity_lowPt_Barrel", "NeutMultiplicity_lowPt_Barrel", 30,0,30);
      mMuMultiplicity_lowPt_Barrel    = dbe_->book1D("MuMultiplicity_lowPt_Barrel", "MuMultiplicity_lowPt_Barrel", 30,0,30);
      mChMultiplicity_mediumPt_Barrel    = dbe_->book1D("ChMultiplicity_mediumPt_Barrel", "ChMultiplicity_mediumPt_Barrel", 30,0,30);
      mNeutMultiplicity_mediumPt_Barrel   = dbe_->book1D("NeutMultiplicity_mediumPt_Barrel", "NeutMultiplicity_mediumPt_Barrel", 30,0,30);
      mMuMultiplicity_mediumPt_Barrel    = dbe_->book1D("MuMultiplicity_mediumPt_Barrel", "MuMultiplicity_mediumPt_Barrel", 30,0,30);
      mChMultiplicity_highPt_Barrel    = dbe_->book1D("ChMultiplicity_highPt_Barrel", "ChMultiplicity_highPt_Barrel", 30,0,30);
      mNeutMultiplicity_highPt_Barrel   = dbe_->book1D("NeutMultiplicity_highPt_Barrel", "NeutMultiplicity_highPt_Barrel", 30,0,30);
      mMuMultiplicity_highPt_Barrel    = dbe_->book1D("MuMultiplicity_highPt_Barrel", "MuMultiplicity_highPt_Barrel", 30,0,30);
      //
      mCHFracVSpT_Barrel= dbe_->bookProfile("CHFracVSpT_Barrel","CHFracVSpT_Barrel",ptBin_, ptMin_, ptMax_,0.,1.2);
      mNHFracVSpT_Barrel= dbe_->bookProfile("NHFracVSpT_Barrel","NHFracVSpT_Barrel",ptBin_, ptMin_, ptMax_,0.,1.2);
      mPhFracVSpT_Barrel= dbe_->bookProfile("PhFracVSpT_Barrel","PhFracVSpT_Barrel",ptBin_, ptMin_, ptMax_,0.,1.2);
      mElFracVSpT_Barrel= dbe_->bookProfile("ElFracVSpT_Barrel","ElFracVSpT_Barrel",ptBin_, ptMin_, ptMax_,0.,1.2);
      mMuFracVSpT_Barrel= dbe_->bookProfile("MuFracVSpT_Barrel","MuFracVSpT_Barrel",ptBin_, ptMin_, ptMax_,0.,1.2);
      mCHFracVSpT_EndCap= dbe_->bookProfile("CHFracVSpT_EndCap","CHFracVSpT_EndCap",ptBin_, ptMin_, ptMax_,0.,1.2);
      mNHFracVSpT_EndCap= dbe_->bookProfile("NHFracVSpT_EndCap","NHFracVSpT_EndCap",ptBin_, ptMin_, ptMax_,0.,1.2);
      mPhFracVSpT_EndCap= dbe_->bookProfile("PhFracVSpT_EndCap","PhFracVSpT_EndCap",ptBin_, ptMin_, ptMax_,0.,1.2);
      mElFracVSpT_EndCap= dbe_->bookProfile("ElFracVSpT_EndCap","ElFracVSpT_EndCap",ptBin_, ptMin_, ptMax_,0.,1.2);
      mMuFracVSpT_EndCap= dbe_->bookProfile("MuFracVSpT_EndCap","MuFracVSpT_EndCap",ptBin_, ptMin_, ptMax_,0.,1.2);
      mHFHFracVSpT_Forward= dbe_->bookProfile("HFHFracVSpT_Forward","HFHFracVSpT_Forward",ptBin_, ptMin_, ptMax_,-0.2,1.2);
      mHFEFracVSpT_Forward= dbe_->bookProfile("HFEFracVSpT_Forward","HFEFracVSpT_Forward",ptBin_, ptMin_, ptMax_,-0.2,1.2);
      //endcap monitoring
      //energy fractions
      mCHFrac_lowPt_EndCap     = dbe_->book1D("CHFrac_lowPt_EndCap", "CHFrac_lowPt_EndCap", 120, -0.1, 1.1);
      mNHFrac_lowPt_EndCap     = dbe_->book1D("NHFrac_lowPt_EndCap", "NHFrac_lowPt_EndCap", 120, -0.1, 1.1);
      mPhFrac_lowPt_EndCap     = dbe_->book1D("PhFrac_lowPt_EndCap", "PhFrac_lowPt_EndCap", 120, -0.1, 1.1);
      mElFrac_lowPt_EndCap     = dbe_->book1D("ElFrac_lowPt_EndCap", "ElFrac_lowPt_EndCap", 120, -0.1, 1.1);
      mMuFrac_lowPt_EndCap     = dbe_->book1D("MuFrac_lowPt_EndCap", "MuFrac_lowPt_EndCap", 120, -0.1, 1.1);
      mCHFrac_mediumPt_EndCap  = dbe_->book1D("CHFrac_mediumPt_EndCap", "CHFrac_mediumPt_EndCap", 120, -0.1, 1.1);
      mNHFrac_mediumPt_EndCap  = dbe_->book1D("NHFrac_mediumPt_EndCap", "NHFrac_mediumPt_EndCap", 120, -0.1, 1.1);
      mPhFrac_mediumPt_EndCap  = dbe_->book1D("PhFrac_mediumPt_EndCap", "PhFrac_mediumPt_EndCap", 120, -0.1, 1.1);
      mElFrac_mediumPt_EndCap  = dbe_->book1D("ElFrac_mediumPt_EndCap", "ElFrac_mediumPt_EndCap", 120, -0.1, 1.1);
      mMuFrac_mediumPt_EndCap  = dbe_->book1D("MuFrac_mediumPt_EndCap", "MuFrac_mediumPt_EndCap", 120, -0.1, 1.1);
      mCHFrac_highPt_EndCap    = dbe_->book1D("CHFrac_highPt_EndCap", "CHFrac_highPt_EndCap", 120, -0.1, 1.1);
      mNHFrac_highPt_EndCap    = dbe_->book1D("NHFrac_highPt_EndCap", "NHFrac_highPt_EndCap", 120, -0.1, 1.1);
      mPhFrac_highPt_EndCap    = dbe_->book1D("PhFrac_highPt_EndCap", "PhFrac_highPt_EndCap", 120, -0.1, 1.1);
      mElFrac_highPt_EndCap    = dbe_->book1D("ElFrac_highPt_EndCap", "ElFrac_highPt_EndCap", 120, -0.1, 1.1);
      mMuFrac_highPt_EndCap    = dbe_->book1D("MuFrac_highPt_EndCap", "MuFrac_highPt_EndCap", 120, -0.1, 1.1);
      //energies
      mCHEn_lowPt_EndCap     = dbe_->book1D("CHEn_lowPt_EndCap", "CHEn_lowPt_EndCap", ptBin_, 0., ptMax_);
      mNHEn_lowPt_EndCap     = dbe_->book1D("NHEn_lowPt_EndCap", "NHEn_lowPt_EndCap", ptBin_, 0., ptMax_);
      mPhEn_lowPt_EndCap     = dbe_->book1D("PhEn_lowPt_EndCap", "PhEn_lowPt_EndCap", ptBin_, 0., ptMax_);
      mElEn_lowPt_EndCap     = dbe_->book1D("ElEn_lowPt_EndCap", "ElEn_lowPt_EndCap", ptBin_, 0., ptMax_);
      mMuEn_lowPt_EndCap     = dbe_->book1D("MuEn_lowPt_EndCap", "MuEn_lowPt_EndCap", ptBin_, 0., ptMax_);
      mCHEn_mediumPt_EndCap  = dbe_->book1D("CHEn_mediumPt_EndCap", "CHEn_mediumPt_EndCap", ptBin_, 0., ptMax_);
      mNHEn_mediumPt_EndCap  = dbe_->book1D("NHEn_mediumPt_EndCap", "NHEn_mediumPt_EndCap", ptBin_, 0., ptMax_);
      mPhEn_mediumPt_EndCap  = dbe_->book1D("PhEn_mediumPt_EndCap", "PhEn_mediumPt_EndCap", ptBin_, 0., ptMax_);
      mElEn_mediumPt_EndCap  = dbe_->book1D("ElEn_mediumPt_EndCap", "ElEn_mediumPt_EndCap", ptBin_, 0., ptMax_);
      mMuEn_mediumPt_EndCap  = dbe_->book1D("MuEn_mediumPt_EndCap", "MuEn_mediumPt_EndCap", ptBin_, 0., ptMax_);
      mCHEn_highPt_EndCap    = dbe_->book1D("CHEn_highPt_EndCap", "CHEn_highPt_EndCap", ptBin_, 0., ptMax_);
      mNHEn_highPt_EndCap    = dbe_->book1D("NHEn_highPt_EndCap", "NHEn_highPt_EndCap", ptBin_, 0., ptMax_);
      mPhEn_highPt_EndCap    = dbe_->book1D("PhEn_highPt_EndCap", "PhEn_highPt_EndCap", ptBin_, 0., ptMax_);
      mElEn_highPt_EndCap    = dbe_->book1D("ElEn_highPt_EndCap", "ElEn_highPt_EndCap", ptBin_, 0., ptMax_);
      mMuEn_highPt_EndCap    = dbe_->book1D("MuEn_highPt_EndCap", "MuEn_highPt_EndCap", ptBin_, 0., ptMax_);
      //multiplicities
      mChMultiplicity_lowPt_EndCap    = dbe_->book1D("ChMultiplicity_lowPt_EndCap", "ChMultiplicity_lowPt_EndCap", 30,0,30);
      mNeutMultiplicity_lowPt_EndCap   = dbe_->book1D("NeutMultiplicity_lowPt_EndCap", "NeutMultiplicity_lowPt_EndCap", 30,0,30);
      mMuMultiplicity_lowPt_EndCap    = dbe_->book1D("MuMultiplicity_lowPt_EndCap", "MuMultiplicity_lowPt_EndCap", 30,0,30);
      mChMultiplicity_mediumPt_EndCap    = dbe_->book1D("ChMultiplicity_mediumPt_EndCap", "ChMultiplicity_mediumPt_EndCap", 30,0,30);
      mNeutMultiplicity_mediumPt_EndCap   = dbe_->book1D("NeutMultiplicity_mediumPt_EndCap", "NeutMultiplicity_mediumPt_EndCap", 30,0,30);
      mMuMultiplicity_mediumPt_EndCap    = dbe_->book1D("MuMultiplicity_mediumPt_EndCap", "MuMultiplicity_mediumPt_EndCap", 30,0,30);
      mChMultiplicity_highPt_EndCap    = dbe_->book1D("ChMultiplicity_highPt_EndCap", "ChMultiplicity_highPt_EndCap", 30,0,30);
      mNeutMultiplicity_highPt_EndCap   = dbe_->book1D("NeutMultiplicity_highPt_EndCap", "NeutMultiplicity_highPt_EndCap", 30,0,30);
      mMuMultiplicity_highPt_EndCap    = dbe_->book1D("MuMultiplicity_highPt_EndCap", "MuMultiplicity_highPt_EndCap", 30,0,30);
      //forward monitoring
      //energy fraction
      mHFEFrac_lowPt_Forward    = dbe_->book1D("HFEFrac_lowPt_Forward", "HFEFrac_lowPt_Forward", 140, -0.2, 1.2);
      mHFHFrac_lowPt_Forward    = dbe_->book1D("HFHFrac_lowPt_Forward", "HFHFrac_lowPt_Forward", 140, -0.2, 1.2);
      mHFEFrac_mediumPt_Forward = dbe_->book1D("HFEFrac_mediumPt_Forward", "HFEFrac_mediumPt_Forward", 140, -0.2, 1.2);
      mHFHFrac_mediumPt_Forward = dbe_->book1D("HFHFrac_mediumPt_Forward", "HFHFrac_mediumPt_Forward", 140, -0.2, 1.2);
      mHFEFrac_highPt_Forward   = dbe_->book1D("HFEFrac_highPt_Forward", "HFEFrac_highPt_Forward", 140, -0.2, 1.2);
      mHFHFrac_highPt_Forward   = dbe_->book1D("HFHFrac_highPt_Forward", "HFHFrac_highPt_Forward", 140, -0.2, 1.2);
      //energies
      mHFEEn_lowPt_Forward    = dbe_->book1D("HFEEn_lowPt_Forward", "HFEEn_lowPt_Forward", ptBin_, 0., ptMax_);
      mHFHEn_lowPt_Forward    = dbe_->book1D("HFHEn_lowPt_Forward", "HFHEn_lowPt_Forward", ptBin_, 0., ptMax_);
      mHFEEn_mediumPt_Forward = dbe_->book1D("HFEEn_mediumPt_Forward", "HFEEn_mediumPt_Forward", ptBin_, 0., ptMax_);
      mHFHEn_mediumPt_Forward = dbe_->book1D("HFHEn_mediumPt_Forward", "HFHEn_mediumPt_Forward", ptBin_, 0., ptMax_);
      mHFEEn_highPt_Forward   = dbe_->book1D("HFEEn_highPt_Forward", "HFEEn_highPt_Forward", ptBin_, 0., ptMax_);
      mHFHEn_highPt_Forward   = dbe_->book1D("HFHEn_highPt_Forward", "HFHEn_highPt_Forward", ptBin_, 0., ptMax_);
      //multiplicities
      mChMultiplicity_lowPt_Forward     = dbe_->book1D("ChMultiplicity_lowPt_Forward", "ChMultiplicity_lowPt_Forward", 30,0,30);
      mNeutMultiplicity_lowPt_Forward    = dbe_->book1D("NeutMultiplicity_lowPt_Forward", "NeutMultiplicity_lowPt_Forward", 30,0,30);
      mChMultiplicity_mediumPt_Forward  = dbe_->book1D("ChMultiplicity_mediumPt_Forward", "ChMultiplicity_mediumPt_Forward", 30,0,30);
      mNeutMultiplicity_mediumPt_Forward = dbe_->book1D("NeutMultiplicity_mediumPt_Forward", "NeutMultiplicity_mediumPt_Forward", 30,0,30);
      mChMultiplicity_highPt_Forward    = dbe_->book1D("ChMultiplicity_highPt_Forward", "ChMultiplicity_highPt_Forward", 30,0,30);
      mNeutMultiplicity_highPt_Forward   = dbe_->book1D("NeutMultiplicity_highPt_Forward", "NeutMultiplicity_highPt_Forward", 30,0,30);
    }
    
    mChargedHadronEnergy = dbe_->book1D("mChargedHadronEnergy", "charged HAD energy",    100, 0, 100);
    mNeutralHadronEnergy = dbe_->book1D("mNeutralHadronEnergy", "neutral HAD energy",    100, 0, 100);
    mChargedEmEnergy     = dbe_->book1D("mChargedEmEnergy ",    "charged EM energy ",    100, 0, 100);
    mChargedMuEnergy     = dbe_->book1D("mChargedMuEnergy",     "charged Mu energy",     100, 0, 100);
    mNeutralEmEnergy     = dbe_->book1D("mNeutralEmEnergy",     "neutral EM energy",     100, 0, 100);
    mChargedMultiplicity = dbe_->book1D("mChargedMultiplicity", "charged multiplicity ", 100, 0, 100);
    mNeutralMultiplicity = dbe_->book1D("mNeutralMultiplicity", "neutral multiplicity",  100, 0, 100);
    mMuonMultiplicity    = dbe_->book1D("mMuonMultiplicity",    "muon multiplicity",     100, 0, 100);
    
    
    // Book NPV profiles
    //----------------------------------------------------------------------------
    mChargedHadronEnergy_profile = dbe_->bookProfile("mChargedHadronEnergy_profile", "charged HAD energy",   nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 100);
    mNeutralHadronEnergy_profile = dbe_->bookProfile("mNeutralHadronEnergy_profile", "neutral HAD energy",   nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 100);
    mChargedEmEnergy_profile     = dbe_->bookProfile("mChargedEmEnergy_profile",     "charged EM energy",    nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 100);
    mChargedMuEnergy_profile     = dbe_->bookProfile("mChargedMuEnergy_profile",     "charged Mu energy",    nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 100);
    mNeutralEmEnergy_profile     = dbe_->bookProfile("mNeutralEmEnergy_profile",     "neutral EM energy",    nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 100);
    mChargedMultiplicity_profile = dbe_->bookProfile("mChargedMultiplicity_profile", "charged multiplicity", nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 100);
    mNeutralMultiplicity_profile = dbe_->bookProfile("mNeutralMultiplicity_profile", "neutral multiplicity", nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 100);
    mMuonMultiplicity_profile    = dbe_->bookProfile("mMuonMultiplicity_profile",    "muon multiplicity",    nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 100);
    
    // Set NPV profiles x-axis title
    //----------------------------------------------------------------------------
    mChargedHadronEnergy_profile->setAxisTitle("nvtx",1);
    mNeutralHadronEnergy_profile->setAxisTitle("nvtx",1);
    mChargedEmEnergy_profile    ->setAxisTitle("nvtx",1);
    mChargedMuEnergy_profile    ->setAxisTitle("nvtx",1);
    mNeutralEmEnergy_profile    ->setAxisTitle("nvtx",1);
    mChargedMultiplicity_profile->setAxisTitle("nvtx",1);
    mNeutralMultiplicity_profile->setAxisTitle("nvtx",1);
    mMuonMultiplicity_profile   ->setAxisTitle("nvtx",1);
    
    mNeutralFraction     = dbe_->book1D("NeutralConstituentsFraction","Neutral Constituens Fraction",100,0,1);

  }

  if(jetCleaningFlag_){
    //so far we have only one additional selection -> implement to make it expandable
    folderNames_.push_back("DiJet");
    //book for each of these selection default histograms
    for (std::vector<std::string>::const_iterator ic = folderNames_.begin();
	 ic != folderNames_.end(); ic++){
      bookMESetSelection(DirName+"/"+*ic);
    }
  }

  dbe_->setCurrentFolder("JetMET");
  lumisecME = dbe_->book1D("lumisec", "lumisec", 500, 0., 500.);
  cleanupME = dbe_->book1D("cleanup", "cleanup", 10, 0., 10.);
  cleanupME->setBinLabel(1,"Primary Vertex");
  cleanupME->setBinLabel(2,"DCS::Pixel");
  cleanupME->setBinLabel(3,"DCS::SiStrip");
  cleanupME->setBinLabel(4,"DCS::ECAL");
  cleanupME->setBinLabel(5,"DCS::ES");
  cleanupME->setBinLabel(6,"DCS::HBHE");
  cleanupME->setBinLabel(7,"DCS::HF");
  cleanupME->setBinLabel(8,"DCS::HO");
  cleanupME->setBinLabel(9,"DCS::Muon");

  verticesME = dbe_->book1D("vertices", "vertices", 100, 0, 100);



}

void JetAnalyzer::bookMESetSelection(std::string DirName)
{
  dbe_->setCurrentFolder(DirName);
  // Generic jet parameters
  mPt           = dbe_->book1D("Pt",           "pt",                 ptBin_,  ptMin_,  ptMax_);
  mEta          = dbe_->book1D("Eta",          "eta",               etaBin_, etaMin_, etaMax_);
  mPhi          = dbe_->book1D("Phi",          "phi",               phiBin_, phiMin_, phiMax_);
  if(!isJPTJet_){
    mConstituents = dbe_->book1D("Constituents", "# of constituents",     50,      0,    100);
  }
  mJetEnergyCorr= dbe_->book1D("JetEnergyCorr", "jet energy correction factor", 50, 0.0,3.0);
  mJetEnergyCorrVSEta= dbe_->bookProfile("JetEnergyCorrVSEta", "jet energy correction factor VS eta", etaBin_, etaMin_,etaMax_, 0.0,3.0);
  mHFrac        = dbe_->book1D("HFrac",        "HFrac",                140,   -0.2,    1.2);
  mEFrac        = dbe_->book1D("EFrac",        "EFrac",                140,   -0.2,    1.2);

  mDPhi                   = dbe_->book1D("DPhi", "dPhi btw the two leading jets", 100, 0., acos(-1.));
  mDijetAsymmetry                   = dbe_->book1D("DijetAsymmetry", "DijetAsymmetry", 100, -1., 1.);
  mDijetBalance                     = dbe_->book1D("DijetBalance",   "DijetBalance",   100, -2., 2.);

  // Book NPV profiles
  //----------------------------------------------------------------------------
  mPt_profile           = dbe_->bookProfile("Pt_profile",           "pt",                nbinsPV_, nPVlow_, nPVhigh_,   ptBin_,  ptMin_,  ptMax_);
  mEta_profile          = dbe_->bookProfile("Eta_profile",          "eta",               nbinsPV_, nPVlow_, nPVhigh_,  etaBin_, etaMin_, etaMax_);
  mPhi_profile          = dbe_->bookProfile("Phi_profile",          "phi",               nbinsPV_, nPVlow_, nPVhigh_,  phiBin_, phiMin_, phiMax_);
  if(!isJPTJet_){
    mConstituents_profile = dbe_->bookProfile("Constituents_profile", "# of constituents", nbinsPV_, nPVlow_, nPVhigh_,      50,      0,    100);
  }
  mHFrac_profile        = dbe_->bookProfile("HFrac_profile",        "HFrac",             nbinsPV_, nPVlow_, nPVhigh_,     140,   -0.2,    1.2);
  mEFrac_profile        = dbe_->bookProfile("EFrac_profile",        "EFrac",             nbinsPV_, nPVlow_, nPVhigh_,     140,   -0.2,    1.2);
  // met NPV profiles x-axis title
  //----------------------------------------------------------------------------
  mPt_profile          ->setAxisTitle("nvtx",1);
  mEta_profile         ->setAxisTitle("nvtx",1);
  mPhi_profile         ->setAxisTitle("nvtx",1);
  if(!isJPTJet_){
    mConstituents_profile->setAxisTitle("nvtx",1);
  }
  mHFrac_profile       ->setAxisTitle("nvtx",1);
  mEFrac_profile       ->setAxisTitle("nvtx",1);

  //
  //--- Calo jet melection only
  if(isCaloJet_) {
    // CaloJet mpecific
    mMaxEInEmTowers         = dbe_->book1D("MaxEInEmTowers", "MaxEInEmTowers", 100, 0, 100);
    mMaxEInHadTowers        = dbe_->book1D("MaxEInHadTowers", "MaxEInHadTowers", 100, 0, 100);
    //JetID variables
    mresEMF                 = dbe_->book1D("resEMF", "resEMF", 50, 0., 1.);
    mN90Hits                = dbe_->book1D("N90Hits", "N90Hits", 50, 0., 50);
    mfHPD                   = dbe_->book1D("fHPD", "fHPD", 50, 0., 1.);
    mfRBX                   = dbe_->book1D("fRBX", "fRBX", 50, 0., 1.);
    
  }

  if(isPFJet_){ 
    //barrel histograms for PFJets
    // energy fractions
    mCHFrac     = dbe_->book1D("CHFrac", "CHFrac", 120, -0.1, 1.1);
    mNHFrac     = dbe_->book1D("NHFrac", "NHFrac", 120, -0.1, 1.1);
    mPhFrac     = dbe_->book1D("PhFrac", "PhFrac", 120, -0.1, 1.1);
    mElFrac     = dbe_->book1D("ElFrac", "ElFrac", 120, -0.1, 1.1);
    mMuFrac     = dbe_->book1D("MuFrac", "MuFrac", 120, -0.1, 1.1);
    mNeutralEmEnergy     = dbe_->book1D("NeutralEmEnergy",     "neutral EM energy",     100, 0, 100);
    mChargedMultiplicity = dbe_->book1D("ChargedMultiplicity", "charged multiplicity ", 100, 0, 100);
    mNeutralMultiplicity = dbe_->book1D("NeutralMultiplicity", "neutral multiplicity",  100, 0, 100);
    mMuonMultiplicity    = dbe_->book1D("MuonMultiplicity",    "muon multiplicity",     100, 0, 100);
    
    
    // Book NPV profiles
    //----------------------------------------------------------------------------
    mCHFrac_profile = dbe_->bookProfile("CHFrac_profile", "charged HAD fraction profile",   nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 1);
    mNHFrac_profile = dbe_->bookProfile("NHFrac_profile", "neutral HAD fraction profile",   nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 1);
    mElFrac_profile     = dbe_->bookProfile("ElFrac_profile",     "Electron Fraction Profile",    nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 1);
    mMuFrac_profile     = dbe_->bookProfile("MuFrac_profile",     "Muon Fraction Profile",    nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 1);
    mPhFrac_profile     = dbe_->bookProfile("PhFrac_profile",     "Photon Fraction Profile",    nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 1);
    mChargedMultiplicity_profile = dbe_->bookProfile("ChargedMultiplicity_profile", "charged multiplicity", nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 100);
    mNeutralMultiplicity_profile = dbe_->bookProfile("NeutralMultiplicity_profile", "neutral multiplicity", nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 100);
    mMuonMultiplicity_profile    = dbe_->bookProfile("MuonMultiplicity_profile",    "muon multiplicity",    nbinsPV_, nPVlow_, nPVhigh_, 100, 0, 100);
    
    // met NPV profiles x-axis title
    //----------------------------------------------------------------------------
    mCHFrac_profile->setAxisTitle("nvtx",1);
    mNHFrac_profile->setAxisTitle("nvtx",1);
    mElFrac_profile    ->setAxisTitle("nvtx",1);
    mMuFrac_profile    ->setAxisTitle("nvtx",1);
    mPhFrac_profile    ->setAxisTitle("nvtx",1);
    mChargedMultiplicity_profile->setAxisTitle("nvtx",1);
    mNeutralMultiplicity_profile->setAxisTitle("nvtx",1);
    mMuonMultiplicity_profile   ->setAxisTitle("nvtx",1);
    
    mNeutralFraction     = dbe_->book1D("NeutralConstituentsFraction","Neutral Constituens Fraction",100,0,1);
  }
 
}

// ***********************************************************
void JetAnalyzer::beginRun(const edm::Run& iRun, const edm::EventSetup& iSetup)
{
  //LogDebug("JetAnalyzer") << "beginRun, run " << run.id();
  //

  if ( highPtJetEventFlag_->on() ) highPtJetEventFlag_->initRun( iRun, iSetup );
  if ( lowPtJetEventFlag_ ->on() ) lowPtJetEventFlag_ ->initRun( iRun, iSetup );

  if (highPtJetEventFlag_->on() && highPtJetEventFlag_->expressionsFromDB(highPtJetEventFlag_->hltDBKey(), iSetup)[0] != "CONFIG_ERROR")
    highPtJetExpr_ = highPtJetEventFlag_->expressionsFromDB(highPtJetEventFlag_->hltDBKey(), iSetup);
  if (lowPtJetEventFlag_->on() && lowPtJetEventFlag_->expressionsFromDB(lowPtJetEventFlag_->hltDBKey(), iSetup)[0] != "CONFIG_ERROR")
    lowPtJetExpr_  = lowPtJetEventFlag_->expressionsFromDB(lowPtJetEventFlag_->hltDBKey(),   iSetup);
//  if (!jetCorrectionService_.empty()){
//    energycorrected=true;
//  }
  //--- htlConfig_
  //processname_="HLT";
  bool changed(true);
  hltInitialized_ = hltConfig_.init(iRun,iSetup,processname_,changed);
  if (!hltInitialized_) {
  //if (!hltConfig_.init(iRun,iSetup,processname_,changed)) {
    processname_ = "FU";
    hltInitialized_ = hltConfig_.init(iRun,iSetup,processname_,changed);
    if(!hltInitialized_){
      //if (!hltConfig_.init(iRun,iSetup,processname_,changed)){
      LogDebug("JetAnalyzer") << "HLTConfigProvider failed to initialize.";
    }
  }
 
}

// ***********************************************************
void JetAnalyzer::endRun(const edm::Run& iRun, const edm::EventSetup& iSetup)
{
}

// ***********************************************************
void JetAnalyzer::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {

  //set general folders first --> change later on for different folders
  if(diJetSelectionFlag_){
    dbe_->setCurrentFolder("JetMET/Jet/DiJets"+mInputCollection_.label());
  }else{
    if(jetCleaningFlag_){
      dbe_->setCurrentFolder("JetMET/Jet/Cleaned"+mInputCollection_.label());
      DirName = "JetMET/Jet/Cleaned"+mInputCollection_.label();
    }else{
      dbe_->setCurrentFolder("JetMET/Jet/Uncleaned"+mInputCollection_.label());
      DirName = "JetMET/Jet/Uncleaned"+mInputCollection_.label();
    }
  }
  
  // *** Fill lumisection ME
  int myLuminosityBlock;
  myLuminosityBlock = iEvent.luminosityBlock();
  lumisecME->Fill(myLuminosityBlock);

  if (myLuminosityBlock<LSBegin_) return;
  if (myLuminosityBlock>LSEnd_ && LSEnd_>0) return;

  // **** Get the TriggerResults container
  edm::Handle<edm::TriggerResults> triggerResults;
  iEvent.getByToken(triggerResultsToken_, triggerResults);
  
  Int_t JetLoPass = 0;
  Int_t JetHiPass = 0;

  if (triggerResults.isValid()){
    const edm::TriggerNames & triggerNames = iEvent.triggerNames(*triggerResults);

    const unsigned int nTrig(triggerNames.size());
    for (unsigned int i=0;i<nTrig;++i)
      {
        if (triggerNames.triggerName(i).find(highPtJetExpr_[0].substr(0,highPtJetExpr_[0].rfind("_v")+2))!=std::string::npos && triggerResults->accept(i))
	  JetHiPass=1;
        else if (triggerNames.triggerName(i).find(lowPtJetExpr_[0].substr(0,lowPtJetExpr_[0].rfind("_v")+2))!=std::string::npos && triggerResults->accept(i))
	  JetLoPass=1;
      }

  }

  if (verbose_)  std::cout << "trigger label " << theTriggerResultsLabel_ << std::endl;


  if (verbose_) {
    std::cout << ">>> Trigger  Lo = " <<  JetLoPass
	      <<             " Hi = " <<  JetHiPass
	      << std::endl;
  }

  // ==========================================================
  //Vertex information
  Handle<VertexCollection> vertexHandle;
  iEvent.getByToken(vertexToken_, vertexHandle);

  if (!vertexHandle.isValid()) {
    LogDebug("") << "CaloMETAnalyzer: Could not find vertex collection" << std::endl;
    if (verbose_) std::cout << "CaloMETAnalyzer: Could not find vertex collection" << std::endl;
  }
  int numPV = 0;
  if ( vertexHandle.isValid() ){
    VertexCollection vertexCollection = *(vertexHandle.product());
    numPV  = vertexCollection.size();
  }
  bool bPrimaryVertex = (bypassAllPVChecks_ || (numPV>0));
  verticesME->Fill(numPV);
  // ==========================================================

  edm::Handle<L1GlobalTriggerReadoutRecord > gtReadoutRecord;
  iEvent.getByToken(gtToken_, gtReadoutRecord);


  if (!gtReadoutRecord.isValid()) {
    LogInfo("JetAnalyzer") << "JetAnalyzer: Could not find GT readout record" << std::endl;
    if (verbose_) std::cout << "JetAnalyzer: Could not find GT readout record product" << std::endl;
  }
  DCSFilterForDCSMonitoring_->filter(iEvent, iSetup);
  if (bPrimaryVertex) cleanupME->Fill(0.5);
  if ( DCSFilterForDCSMonitoring_->passPIX      ) cleanupME->Fill(1.5);
  if ( DCSFilterForDCSMonitoring_->passSiStrip  ) cleanupME->Fill(2.5);
  if ( DCSFilterForDCSMonitoring_->passECAL     ) cleanupME->Fill(3.5);
  if ( DCSFilterForDCSMonitoring_->passES       ) cleanupME->Fill(4.5);
  if ( DCSFilterForDCSMonitoring_->passHBHE     ) cleanupME->Fill(5.5);
  if ( DCSFilterForDCSMonitoring_->passHF       ) cleanupME->Fill(6.5);
  if ( DCSFilterForDCSMonitoring_->passHO       ) cleanupME->Fill(7.5);
  if ( DCSFilterForDCSMonitoring_->passMuon     ) cleanupME->Fill(8.5);
  
  edm::Handle<CaloJetCollection> caloJets;
  edm::Handle<JPTJetCollection> jptJets;
  edm::Handle<PFJetCollection> pfJets;

  if (isCaloJet_) iEvent.getByToken(caloJetsToken_, caloJets);
  if (isJPTJet_) iEvent.getByToken(jptJetsToken_, jptJets);
  if (isPFJet_) iEvent.getByToken(pfJetsToken_, pfJets);

  edm::Handle< edm::ValueMap<reco::JetID> >jetID_ValueMap_Handle;
  if(isJPTJet_ || isCaloJet_){
    iEvent.getByToken(jetID_ValueMapToken_,jetID_ValueMap_Handle);
  }

  //check for collections AND DCS filters
  bool dcsDecision = DCSFilterForJetMonitoring_->filter(iEvent, iSetup);
  bool jetCollectionIsValid = false;
  if (isCaloJet_)  jetCollectionIsValid = caloJets.isValid();
  if (isJPTJet_)   jetCollectionIsValid = jptJets.isValid();
  if (isPFJet_)    jetCollectionIsValid = pfJets.isValid();


  //FIXME: Robert, check this. 
  if (jetCleaningFlag_ && (!jetCollectionIsValid || !bPrimaryVertex || !dcsDecision)) return;

  unsigned int collSize=-1;
  if (isCaloJet_)  collSize = caloJets->size();
  if (isJPTJet_) {
    collSize=jptJets->size();
    if(collSize>0){
      //update the track propagator and strip noise calculator
      trackPropagator_->update(iSetup);
      //sOverNCalculator_->update(iSetup);
    }
  }
  if (isPFJet_) collSize=pfJets->size();

  double scale=-1;
  //now start changes for jets
  std::vector<Jet> corJets;
  corJets.clear();

  //maybe not most elegant solution, but works for sure
  unsigned int ind1=-1;
  double pt1=-1;
  unsigned int ind2=-1;
  double pt2=-1;
  unsigned int ind3=-1;
  double pt3=-1;

  bool cleaned_first_jet=false;
  bool cleaned_second_jet=false;
  bool cleaned_third_jet=false;

  //now start changes for jets
  std::vector<Jet> recoJets;
  recoJets.clear();

  int numofjets=0;

  
  for (unsigned int ijet=0; ijet<collSize; ijet++) {
    //bool thiscleaned=false;
    Jet correctedJet;
    bool pass_uncorrected=false;
    bool pass_corrected=false;
    if (isCaloJet_){
      correctedJet=(*caloJets)[ijet];
    }
    if (isJPTJet_){
      correctedJet=(*jptJets)[ijet];
    }
    if (isPFJet_){
      correctedJet=(*pfJets)[ijet];
    }
    if(correctedJet.pt()>ptThresholdUnc_){
      pass_uncorrected=true;
    }
    if (!jetCorrectionService_.empty()) {
      const JetCorrector* corrector = JetCorrector::getJetCorrector(jetCorrectionService_, iSetup);
      //for (unsigned ijet=0; ijet<recoJets.size(); ijet++) {
      
      if (isCaloJet_){
        scale = corrector->correction((*caloJets)[ijet], iEvent, iSetup);
      }
      if (isJPTJet_){
        scale = corrector->correction((*jptJets)[ijet], iEvent, iSetup);
      }
      if (isPFJet_){ 
        scale = corrector->correction((*pfJets)[ijet], iEvent, iSetup);
      }
      correctedJet.scaleEnergy(scale);	    
    }

    if(correctedJet.pt()> ptThreshold_){
      pass_corrected=true;
    }
    
    if (!pass_corrected && !pass_uncorrected) continue;
    //fill only corrected jets -> check ID for uncorrected jets
    if(pass_corrected){
      recoJets.push_back(correctedJet);
    }
    if(!diJetSelectionFlag_ ){
      bool jetpassid=true;
      bool Thiscleaned=true;
      //jet ID for calojets
      if (isCaloJet_) {
	reco::CaloJetRef calojetref(caloJets, ijet);
	reco::JetID jetID = (*jetID_ValueMap_Handle)[calojetref];
	jetpassid = jetIDFunctor((*caloJets)[ijet], jetID);
	if(jetCleaningFlag_){
	  Thiscleaned=jetpassid;
	}
	if(Thiscleaned && pass_uncorrected){
	  mPt_uncor = dbe_->get(DirName+"/"+"Pt_uncor"); if (mPt_uncor && mPt_uncor->getRootObject())   mPt_uncor->Fill ((*caloJets)[ijet].pt());
	  mEta_uncor = dbe_->get(DirName+"/"+"Eta_uncor"); if (mEta_uncor && mEta_uncor->getRootObject()) mEta_uncor->Fill ((*caloJets)[ijet].eta());
	  mPhi_uncor = dbe_->get(DirName+"/"+"Phi_uncor"); if (mPhi_uncor && mPhi_uncor->getRootObject()) mPhi_uncor->Fill ((*caloJets)[ijet].phi());
	  if(!isJPTJet_){
	    mConstituents_uncor = dbe_->get(DirName+"/"+"Constituents_uncor"); if (mConstituents_uncor && mConstituents_uncor->getRootObject()) mConstituents_uncor->Fill ((*caloJets)[ijet].nConstituents());
	  }
	}
	//now do calojet specific fractions and histograms ->H and E fracs
	if(Thiscleaned && pass_corrected){//if cleaning requested->jet passes a loose ID
	  mHFrac = dbe_->get(DirName+"/"+"HFrac"); if (mHFrac && mHFrac->getRootObject())        mHFrac->Fill ((*caloJets)[ijet].energyFractionHadronic());
	  mEFrac = dbe_->get(DirName+"/"+"EFrac"); if (mEFrac && mHFrac->getRootObject())        mEFrac->Fill ((*caloJets)[ijet].emEnergyFraction());
	  mHFrac_profile = dbe_->get(DirName+"/"+"HFrac_profile"); if (mHFrac_profile && mHFrac_profile->getRootObject())        mHFrac_profile       ->Fill(numPV, (*caloJets)[ijet].energyFractionHadronic());
	  mEFrac_profile = dbe_->get(DirName+"/"+"EFrac_profile"); if (mEFrac_profile && mEFrac_profile->getRootObject())        mEFrac_profile       ->Fill(numPV, (*caloJets)[ijet].emEnergyFraction());
	  if (fabs((*caloJets)[ijet].eta()) <= 1.3) {	
	    mHFrac_Barrel = dbe_->get(DirName+"/"+"HFrac_Barrel"); if (mHFrac_Barrel && mHFrac_Barrel->getRootObject())           mHFrac_Barrel->Fill((*caloJets)[ijet].energyFractionHadronic());	
	    mEFrac_Barrel = dbe_->get(DirName+"/"+"EFrac_Barrel"); if (mEFrac_Barrel && mEFrac_Barrel->getRootObject())           mEFrac_Barrel->Fill((*caloJets)[ijet].emEnergyFraction());	
	  }else if(fabs((*caloJets)[ijet].eta()) <3.0){
	    mHFrac_EndCap = dbe_->get(DirName+"/"+"HFrac_EndCap"); if (mHFrac_EndCap && mHFrac_EndCap->getRootObject())           mHFrac_EndCap->Fill((*caloJets)[ijet].energyFractionHadronic());	
	    mEFrac_EndCap = dbe_->get(DirName+"/"+"EFrac_EndCap"); if (mEFrac_EndCap && mEFrac_EndCap->getRootObject())           mEFrac_EndCap->Fill((*caloJets)[ijet].emEnergyFraction());
	  }else{
	    mHFrac_Forward = dbe_->get(DirName+"/"+"HFrac_Forward"); if (mHFrac_Forward && mHFrac_Forward->getRootObject())           mHFrac_Forward->Fill((*caloJets)[ijet].energyFractionHadronic());	
	    mEFrac_Forward = dbe_->get(DirName+"/"+"EFrac_Forward"); if (mEFrac_Forward && mEFrac_Forward->getRootObject())           mEFrac_Forward->Fill((*caloJets)[ijet].emEnergyFraction());
	  }
	  mMaxEInEmTowers = dbe_->get(DirName+"/"+"MaxEInEmTowers"); if (mMaxEInEmTowers && mMaxEInEmTowers->getRootObject())  mMaxEInEmTowers->Fill ((*caloJets)[ijet].maxEInEmTowers());
	  mMaxEInHadTowers = dbe_->get(DirName+"/"+"MaxEInHadTowers"); if (mMaxEInHadTowers && mMaxEInHadTowers->getRootObject()) mMaxEInHadTowers->Fill ((*caloJets)[ijet].maxEInHadTowers());
	  
	  mHadEnergyInHO = dbe_->get(DirName+"/"+"HadEnergyInHO"); if (mHadEnergyInHO && mHadEnergyInHO->getRootObject())   mHadEnergyInHO->Fill ((*caloJets)[ijet].hadEnergyInHO());
	  mHadEnergyInHB = dbe_->get(DirName+"/"+"HadEnergyInHB"); if (mHadEnergyInHB && mHadEnergyInHB->getRootObject())   mHadEnergyInHB->Fill ((*caloJets)[ijet].hadEnergyInHB());
	  mHadEnergyInHF = dbe_->get(DirName+"/"+"HadEnergyInHF"); if (mHadEnergyInHF && mHadEnergyInHF->getRootObject())   mHadEnergyInHF->Fill ((*caloJets)[ijet].hadEnergyInHF());
	  mHadEnergyInHE = dbe_->get(DirName+"/"+"HadEnergyInHE"); if (mHadEnergyInHE && mHadEnergyInHE->getRootObject())   mHadEnergyInHE->Fill ((*caloJets)[ijet].hadEnergyInHE());
	  mEmEnergyInEB = dbe_->get(DirName+"/"+"EmEnergyInEB"); if (mEmEnergyInEB && mEmEnergyInEB->getRootObject())    mEmEnergyInEB->Fill ((*caloJets)[ijet].emEnergyInEB());
	  mEmEnergyInEE = dbe_->get(DirName+"/"+"EmEnergyInEE"); if (mEmEnergyInEE && mEmEnergyInEE->getRootObject())    mEmEnergyInEE->Fill ((*caloJets)[ijet].emEnergyInEE());
	  mEmEnergyInHF = dbe_->get(DirName+"/"+"EmEnergyInHF"); if (mEmEnergyInHF && mEmEnergyInHF->getRootObject())    mEmEnergyInHF->Fill ((*caloJets)[ijet].emEnergyInHF());
	  
	  mN90Hits = dbe_->get(DirName+"/"+"N90Hits"); if (mN90Hits && mN90Hits->getRootObject()) mN90Hits->Fill (jetID.n90Hits);
	  mfHPD = dbe_->get(DirName+"/"+"fHPD"); if (mfHPD && mfHPD->getRootObject())             mfHPD->Fill (jetID.fHPD);
	  mresEMF = dbe_->get(DirName+"/"+"resEMF"); if (mresEMF && mresEMF->getRootObject())     mresEMF->Fill (jetID.restrictedEMF);
	  mfRBX = dbe_->get(DirName+"/"+"fRBX"); if (mfRBX && mfRBX->getRootObject())             mfRBX->Fill (jetID.fRBX);
	}
      }
      if(isJPTJet_){
	const edm::RefToBase<reco::Jet>&  rawJet = (*jptJets)[ijet].getCaloJetRef();
	//change that step here
	//check jet is correctable by JPT
	//if ( fabs(rawJet->eta()) > 2.1) return;
	
	try {
	  const reco::CaloJet *rawCaloJet = dynamic_cast<const reco::CaloJet*>(&*rawJet);
	  reco::CaloJetRef const theCaloJetRef = (rawJet).castTo<reco::CaloJetRef>();
	  reco::JetID jetID = (*jetID_ValueMap_Handle)[theCaloJetRef];
	  jetpassid = jetIDFunctor(*rawCaloJet, jetID);
	  if(jetCleaningFlag_){
	    Thiscleaned = jetpassid;
	  }
	  if(Thiscleaned /*&&  ( fabs(rawJet->eta()) < 2.1)*/ && pass_corrected){
	    mN90Hits = dbe_->get(DirName+"/"+"N90Hits"); if (mN90Hits && mN90Hits->getRootObject())   mN90Hits->Fill (jetID.n90Hits);
	    mfHPD = dbe_->get(DirName+"/"+"fHPD"); if (mfHPD && mfHPD->getRootObject())               mfHPD->Fill (jetID.fHPD);
	    mresEMF = dbe_->get(DirName+"/"+"resEMF"); if (mresEMF && mresEMF->getRootObject())       mresEMF->Fill (jetID.restrictedEMF);
	    mfRBX = dbe_->get(DirName+"/"+"fRBX"); if (mfRBX && mfRBX->getRootObject())               mfRBX->Fill (jetID.fRBX);
	  }
	} catch (const std::bad_cast&) {
	  edm::LogError("JetPlusTrackDQM") << "Failed to cast raw jet to CaloJet. JPT Jet does not appear to have been built from a CaloJet. "
					   << "Histograms not filled. ";
	  return;
	}
	//plot JPT specific variables for <2.1 jets
	if(Thiscleaned && pass_uncorrected /*&&  ( fabs(rawJet->eta()) < 2.1)*/ ){
	  mPt_uncor = dbe_->get(DirName+"/"+"Pt_uncor"); if (mPt_uncor && mPt_uncor->getRootObject()) if (mPt_uncor)   mPt_uncor->Fill ((*jptJets)[ijet].pt());
	  mEta_uncor = dbe_->get(DirName+"/"+"Eta_uncor"); if (mEta_uncor && mEta_uncor->getRootObject()) mEta_uncor->Fill ((*jptJets)[ijet].eta());
	  mPhi_uncor = dbe_->get(DirName+"/"+"Phi_uncor"); if (mPhi_uncor && mPhi_uncor->getRootObject()) mPhi_uncor->Fill ((*jptJets)[ijet].phi());
	  if(!isJPTJet_){
	    mConstituents_uncor = dbe_->get(DirName+"/"+"Constituents_uncor"); if (mConstituents_uncor && mConstituents_uncor->getRootObject()) mConstituents_uncor->Fill ((*jptJets)[ijet].nConstituents());
	  }
	}
	if(Thiscleaned &&  /*( fabs(rawJet->eta()) < 2.1) && */pass_corrected){
	  mHFrac = dbe_->get(DirName+"/"+"HFrac"); if (mHFrac && mHFrac->getRootObject()) if (mHFrac)        mHFrac->Fill ((*jptJets)[ijet].chargedHadronEnergyFraction()+(*jptJets)[ijet].neutralHadronEnergyFraction());
	  //if (mEFrac)        mEFrac->Fill ((*jptJets)[ijet].chargedEmEnergyFraction() +(*jptJets)[ijet].neutralEmEnergyFraction());
	  mEFrac = dbe_->get(DirName+"/"+"EFrac"); if (mEFrac && mHFrac->getRootObject())       mEFrac->Fill (1.-(*jptJets)[ijet].chargedHadronEnergyFraction()-(*jptJets)[ijet].neutralHadronEnergyFraction());
	  mHFrac_profile = dbe_->get(DirName+"/"+"HFrac_profile"); if (mHFrac_profile && mHFrac_profile->getRootObject())  mHFrac_profile  ->Fill(numPV, (*jptJets)[ijet].chargedHadronEnergyFraction()+(*jptJets)[ijet].neutralHadronEnergyFraction());
	  mEFrac_profile = dbe_->get(DirName+"/"+"EFrac_profile"); if (mEFrac_profile && mEFrac_profile->getRootObject())  mEFrac_profile  ->Fill(numPV, 1.-(*jptJets)[ijet].chargedHadronEnergyFraction()-(*jptJets)[ijet].neutralHadronEnergyFraction());
	  if (fabs((*jptJets)[ijet].eta()) <= 1.3) {	
	    mHFrac_Barrel = dbe_->get(DirName+"/"+"HFrac_Barrel"); if (mHFrac_Barrel && mHFrac_Barrel->getRootObject())  mHFrac_Barrel->Fill((*jptJets)[ijet].chargedHadronEnergyFraction()+(*jptJets)[ijet].neutralHadronEnergyFraction());	
	    mEFrac_Barrel = dbe_->get(DirName+"/"+"EFrac_Barrel"); if (mEFrac_Barrel && mEFrac_Barrel->getRootObject())  mEFrac_Barrel->Fill(1.-(*jptJets)[ijet].chargedHadronEnergyFraction()-(*jptJets)[ijet].neutralHadronEnergyFraction());	
	  }else if(fabs((*jptJets)[ijet].eta()) <3.0){
	    mHFrac_EndCap = dbe_->get(DirName+"/"+"HFrac_EndCap"); if (mHFrac_EndCap && mHFrac_EndCap->getRootObject())     mHFrac_EndCap->Fill((*jptJets)[ijet].chargedHadronEnergyFraction()+(*jptJets)[ijet].neutralHadronEnergyFraction());	
	    mEFrac_EndCap = dbe_->get(DirName+"/"+"EFrac_EndCap"); if (mEFrac_EndCap && mEFrac_EndCap->getRootObject())     mEFrac_EndCap->Fill(1.-(*jptJets)[ijet].chargedHadronEnergyFraction()-(*jptJets)[ijet].neutralHadronEnergyFraction());
	  }else{
	    mHFrac_Forward = dbe_->get(DirName+"/"+"HFrac_Forward"); if (mHFrac_Forward && mHFrac_Forward->getRootObject())   mHFrac_Forward->Fill((*jptJets)[ijet].chargedHadronEnergyFraction()+(*jptJets)[ijet].neutralHadronEnergyFraction());	
	    mEFrac_Forward = dbe_->get(DirName+"/"+"EFrac_Forward"); if (mEFrac_Forward && mEFrac_Forward->getRootObject())      mEFrac_Forward->Fill(1.-(*jptJets)[ijet].chargedHadronEnergyFraction()-(*jptJets)[ijet].neutralHadronEnergyFraction());
	  }
	  mE = dbe_->get(DirName+"/"+"E"); if (mE && mE->getRootObject()) mE->Fill ((*jptJets)[ijet].energy());	
	  mPx = dbe_->get(DirName+"/"+"Px"); if (mPx && mPx->getRootObject()) mPx->Fill ((*jptJets)[ijet].px());	
	  mPy = dbe_->get(DirName+"/"+"Py"); if (mPy && mPy->getRootObject()) mPy->Fill ((*jptJets)[ijet].py());	
	  mPz = dbe_->get(DirName+"/"+"Pz"); if (mPz && mPz->getRootObject()) mPz->Fill ((*jptJets)[ijet].pz());	
	  mP = dbe_->get(DirName+"/"+"P"); if (mP && mP->getRootObject())     mP->Fill ((*jptJets)[ijet].p());	
	  mEt = dbe_->get(DirName+"/"+"Et"); if (mEt && mEt->getRootObject()) mEt->Fill ((*jptJets)[ijet].et());
	  mnTracks = dbe_->get(DirName+"/"+"nTracks"); if (mnTracks && mnTracks->getRootObject()) mnTracks->Fill((*jptJets)[ijet].chargedMultiplicity());
	  mnTracksVSJetPt = dbe_->get(DirName+"/"+"nTracksVSJetPt"); if (mnTracksVSJetPt && mEt->getRootObject()) mnTracksVSJetPt->Fill(rawJet->pt(),(*jptJets)[ijet].chargedMultiplicity());
	  mnTracksVSJetEta = dbe_->get(DirName+"/"+"nTracksVSJetEta"); if (mnTracksVSJetEta && mnTracksVSJetEta->getRootObject()) mnTracksVSJetEta->Fill(rawJet->eta(),(*jptJets)[ijet].chargedMultiplicity());
	  const reco::TrackRefVector& pionsInVertexInCalo = (*jptJets)[ijet].getPionsInVertexInCalo();
	  const reco::TrackRefVector& pionsInVertexOutCalo = (*jptJets)[ijet].getPionsInVertexOutCalo();
	  const reco::TrackRefVector& pionsOutVertexInCalo = (*jptJets)[ijet].getPionsOutVertexInCalo();
	  const reco::TrackRefVector& muonsInVertexInCalo = (*jptJets)[ijet].getMuonsInVertexInCalo();
	  const reco::TrackRefVector& muonsInVertexOutCalo = (*jptJets)[ijet].getMuonsInVertexOutCalo();
	  const reco::TrackRefVector& muonsOutVertexInCalo = (*jptJets)[ijet].getMuonsOutVertexInCalo();
	  const reco::TrackRefVector& electronsInVertexInCalo = (*jptJets)[ijet].getElecsInVertexInCalo();
	  const reco::TrackRefVector& electronsInVertexOutCalo = (*jptJets)[ijet].getElecsInVertexOutCalo();
	  const reco::TrackRefVector& electronsOutVertexInCalo = (*jptJets)[ijet].getElecsOutVertexInCalo();
	  
	  mnallPionTracksPerJet = dbe_->get(DirName+"/"+"nallPionTracks"); if(mnallPionTracksPerJet && mnallPionTracksPerJet->getRootObject()) mnallPionTracksPerJet->Fill(pionsInVertexInCalo.size()+pionsInVertexOutCalo.size()+pionsOutVertexInCalo.size());
	  mnInVertexInCaloPionTracksPerJet = dbe_->get(DirName+"/"+"nInVertexInCaloPionTracks"); if(mnInVertexInCaloPionTracksPerJet && mnInVertexInCaloPionTracksPerJet->getRootObject()) mnInVertexInCaloPionTracksPerJet->Fill(pionsInVertexInCalo.size());
	  mnOutVertexInCaloPionTracksPerJet = dbe_->get(DirName+"/"+"nOutVertexInCaloPionTracks"); if(mnOutVertexInCaloPionTracksPerJet && mnOutVertexInCaloPionTracksPerJet->getRootObject()) mnOutVertexInCaloPionTracksPerJet->Fill(pionsOutVertexInCalo.size());
	  mnInVertexOutCaloPionTracksPerJet = dbe_->get(DirName+"/"+"nInVertexOutCaloPionTracks"); if(mnInVertexOutCaloPionTracksPerJet && mnInVertexOutCaloPionTracksPerJet->getRootObject()) mnInVertexOutCaloPionTracksPerJet->Fill(pionsInVertexOutCalo.size());
	  
	  for (reco::TrackRefVector::const_iterator iTrack = pionsInVertexInCalo.begin(); iTrack != pionsInVertexInCalo.end(); ++iTrack) {
	    mallPionTracksPt = dbe_->get(DirName+"/"+"allPionTracksPt"); if(mallPionTracksPt && mallPionTracksPt->getRootObject()) mallPionTracksPt->Fill((*iTrack)->pt());
	    mallPionTracksEta = dbe_->get(DirName+"/"+"allPionTracksEta"); if(mallPionTracksEta && mallPionTracksEta->getRootObject()) mallPionTracksEta->Fill((*iTrack)->eta());
	    mallPionTracksPhi = dbe_->get(DirName+"/"+"allPionTracksPhi"); if(mallPionTracksPhi && mallPionTracksPhi->getRootObject()) mallPionTracksPhi->Fill((*iTrack)->phi());
	    mallPionTracksPtVSEta = dbe_->get(DirName+"/"+"allPionTracksPtVSEta"); if(mallPionTracksPtVSEta && mallPionTracksPtVSEta->getRootObject()) mallPionTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    mInVertexInCaloPionTracksPt = dbe_->get(DirName+"/"+"InVertexInCaloPionTracksPt"); if(mInVertexInCaloPionTracksPt && mInVertexInCaloPionTracksPt->getRootObject()) mInVertexInCaloPionTracksPt->Fill((*iTrack)->pt());
	    mInVertexInCaloPionTracksEta = dbe_->get(DirName+"/"+"InVertexInCaloPionTracksEta"); if(mInVertexInCaloPionTracksEta && mInVertexInCaloPionTracksEta->getRootObject()) mInVertexInCaloPionTracksEta->Fill((*iTrack)->eta());
	    mInVertexInCaloPionTracksPhi = dbe_->get(DirName+"/"+"InVertexInCaloPionTracksPhi"); if(mInVertexInCaloPionTracksPhi && mInVertexInCaloPionTracksPhi->getRootObject()) mInVertexInCaloPionTracksPhi->Fill((*iTrack)->phi());
	    mInVertexInCaloPionTracksPtVSEta = dbe_->get(DirName+"/"+"InVertexInCaloPionTracksPtVSEta"); if(mInVertexInCaloPionTracksPtVSEta && mInVertexInCaloPionTracksPtVSEta->getRootObject()) mInVertexInCaloPionTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    const double trackDirectionJetDR = deltaR(rawJet->eta(),rawJet->phi(),(*iTrack)->eta(),(*iTrack)->phi());
	    mInCaloTrackDirectionJetDRHisto_= dbe_->get(DirName+"/"+"InCaloTrackDirectionJetDR"); if(mInCaloTrackDirectionJetDRHisto_ && mInCaloTrackDirectionJetDRHisto_ ->getRootObject()) if(mInCaloTrackDirectionJetDRHisto_)mInCaloTrackDirectionJetDRHisto_->Fill(trackDirectionJetDR);
	    math::XYZPoint point =trackPropagator_->impactPoint(**iTrack);
	    const double impactPointJetDR = deltaR(rawJet->eta(),rawJet->phi(), point.Eta(),point.Phi());
	     mInVertexTrackImpactPointJetDRHisto_= dbe_->get(DirName+"/"+"InVertexTrackImpactPointJetDR"); if( mInVertexTrackImpactPointJetDRHisto_ &&  mInVertexTrackImpactPointJetDRHisto_ ->getRootObject()) mInVertexTrackImpactPointJetDRHisto_->Fill(impactPointJetDR);
	  }
	  for (reco::TrackRefVector::const_iterator iTrack = pionsInVertexOutCalo.begin(); iTrack != pionsInVertexOutCalo.end(); ++iTrack) {
	    mallPionTracksPt = dbe_->get(DirName+"/"+"allPionTracksPt"); if(mallPionTracksPt && mallPionTracksPt->getRootObject()) mallPionTracksPt->Fill((*iTrack)->pt());
	    mallPionTracksEta = dbe_->get(DirName+"/"+"allPionTracksEta"); if(mallPionTracksEta && mallPionTracksEta->getRootObject()) mallPionTracksEta->Fill((*iTrack)->eta());
	    mallPionTracksPhi = dbe_->get(DirName+"/"+"allPionTracksPhi"); if(mallPionTracksPhi && mallPionTracksPhi->getRootObject()) mallPionTracksPhi->Fill((*iTrack)->phi());
	    mallPionTracksPtVSEta = dbe_->get(DirName+"/"+"allPionTracksPtVSEta"); if(mallPionTracksPtVSEta && mallPionTracksPtVSEta->getRootObject()) mallPionTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    mInVertexOutCaloPionTracksPt = dbe_->get(DirName+"/"+"InVertexOutCaloPionTracksPt"); if(mInVertexOutCaloPionTracksPt && mInVertexOutCaloPionTracksPt->getRootObject()) mInVertexOutCaloPionTracksPt->Fill((*iTrack)->pt());
	    mInVertexOutCaloPionTracksEta = dbe_->get(DirName+"/"+"InVertexOutCaloPionTracksEta"); if(mInVertexOutCaloPionTracksEta && mInVertexOutCaloPionTracksEta->getRootObject()) mInVertexOutCaloPionTracksEta->Fill((*iTrack)->eta());
	    mInVertexOutCaloPionTracksPhi = dbe_->get(DirName+"/"+"InVertexOutCaloPionTracksPhi"); if(mInVertexOutCaloPionTracksPhi && mInVertexOutCaloPionTracksPhi->getRootObject()) mInVertexOutCaloPionTracksPhi->Fill((*iTrack)->phi());
	    mInVertexOutCaloPionTracksPtVSEta = dbe_->get(DirName+"/"+"InVertexOutCaloPionTracksPtVSEta"); if(mInVertexOutCaloPionTracksPtVSEta && mInVertexOutCaloPionTracksPtVSEta->getRootObject()) mInVertexOutCaloPionTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    const double trackDirectionJetDR = deltaR(rawJet->eta(),rawJet->phi(),(*iTrack)->eta(),(*iTrack)->phi());
	    mOutCaloTrackDirectionJetDRHisto_= dbe_->get(DirName+"/"+"OutCaloTrackDirectionJetDR"); if(mOutCaloTrackDirectionJetDRHisto_ && mOutCaloTrackDirectionJetDRHisto_ ->getRootObject()) if(mOutCaloTrackDirectionJetDRHisto_)mOutCaloTrackDirectionJetDRHisto_->Fill(trackDirectionJetDR);
	    math::XYZPoint point =trackPropagator_->impactPoint(**iTrack);
	    const double impactPointJetDR = deltaR(rawJet->eta(),rawJet->phi(), point.Eta(),point.Phi());
	     mInVertexTrackImpactPointJetDRHisto_= dbe_->get(DirName+"/"+"InVertexTrackImpactPointJetDR"); if( mInVertexTrackImpactPointJetDRHisto_ &&  mInVertexTrackImpactPointJetDRHisto_ ->getRootObject()) mInVertexTrackImpactPointJetDRHisto_->Fill(impactPointJetDR);
	  }
	  for (reco::TrackRefVector::const_iterator iTrack = pionsOutVertexInCalo.begin(); iTrack != pionsOutVertexInCalo.end(); ++iTrack) {
	    mallPionTracksPt = dbe_->get(DirName+"/"+"allPionTracksPt"); if(mallPionTracksPt && mallPionTracksPt->getRootObject()) mallPionTracksPt->Fill((*iTrack)->pt());
	    mallPionTracksEta = dbe_->get(DirName+"/"+"allPionTracksEta"); if(mallPionTracksEta && mallPionTracksEta->getRootObject()) mallPionTracksEta->Fill((*iTrack)->eta());
	    mallPionTracksPhi = dbe_->get(DirName+"/"+"allPionTracksPhi"); if(mallPionTracksPhi && mallPionTracksPhi->getRootObject()) mallPionTracksPhi->Fill((*iTrack)->phi());
	    mallPionTracksPtVSEta = dbe_->get(DirName+"/"+"allPionTracksPtVSEta"); if(mallPionTracksPtVSEta && mallPionTracksPtVSEta->getRootObject()) mallPionTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    mOutVertexInCaloPionTracksPt = dbe_->get(DirName+"/"+"OutVertexInCaloPionTracksPt"); if(mOutVertexInCaloPionTracksPt && mOutVertexInCaloPionTracksPt->getRootObject()) mOutVertexInCaloPionTracksPt->Fill((*iTrack)->pt());
	    mOutVertexInCaloPionTracksEta = dbe_->get(DirName+"/"+"OutVertexInCaloPionTracksEta"); if(mOutVertexInCaloPionTracksEta && mOutVertexInCaloPionTracksEta->getRootObject()) mOutVertexInCaloPionTracksEta->Fill((*iTrack)->eta());
	    mOutVertexInCaloPionTracksPhi = dbe_->get(DirName+"/"+"OutVertexInCaloPionTracksPhi"); if(mOutVertexInCaloPionTracksPhi && mOutVertexInCaloPionTracksPhi->getRootObject()) mOutVertexInCaloPionTracksPhi->Fill((*iTrack)->phi());
	    mOutVertexInCaloPionTracksPtVSEta = dbe_->get(DirName+"/"+"OutVertexInCaloPionTracksPtVSEta"); if(mOutVertexInCaloPionTracksPtVSEta && mOutVertexInCaloPionTracksPtVSEta->getRootObject()) mOutVertexInCaloPionTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    const double trackDirectionJetDR = deltaR(rawJet->eta(),rawJet->phi(),(*iTrack)->eta(),(*iTrack)->phi());
	    mInCaloTrackDirectionJetDRHisto_= dbe_->get(DirName+"/"+"InCaloTrackDirectionJetDR"); if(mInCaloTrackDirectionJetDRHisto_ && mInCaloTrackDirectionJetDRHisto_ ->getRootObject()) if(mInCaloTrackDirectionJetDRHisto_)mInCaloTrackDirectionJetDRHisto_->Fill(trackDirectionJetDR);
	    math::XYZPoint point =trackPropagator_->impactPoint(**iTrack);
	    const double impactPointJetDR = deltaR(rawJet->eta(),rawJet->phi(), point.Eta(),point.Phi());
	     mOutVertexTrackImpactPointJetDRHisto_= dbe_->get(DirName+"/"+"OutVertexTrackImpactPointJetDR"); if( mOutVertexTrackImpactPointJetDRHisto_ &&  mOutVertexTrackImpactPointJetDRHisto_ ->getRootObject()) mOutVertexTrackImpactPointJetDRHisto_->Fill(impactPointJetDR);
	  }
	  //muon track histos
	  mnallMuonTracksPerJet = dbe_->get(DirName+"/"+"nallMuonTracks"); if(mnallMuonTracksPerJet && mnallMuonTracksPerJet->getRootObject()) mnallMuonTracksPerJet->Fill(muonsInVertexInCalo.size()+muonsInVertexOutCalo.size()+muonsOutVertexInCalo.size());
	  mnInVertexInCaloMuonTracksPerJet = dbe_->get(DirName+"/"+"nInVertexInCaloMuonTracks"); if(mnInVertexInCaloMuonTracksPerJet && mnInVertexInCaloMuonTracksPerJet->getRootObject()) mnInVertexInCaloMuonTracksPerJet->Fill(muonsInVertexInCalo.size());
	  mnOutVertexInCaloMuonTracksPerJet = dbe_->get(DirName+"/"+"nOutVertexInCaloMuonTracks"); if(mnOutVertexInCaloMuonTracksPerJet && mnOutVertexInCaloMuonTracksPerJet->getRootObject()) mnOutVertexInCaloMuonTracksPerJet->Fill(muonsOutVertexInCalo.size());
	  mnInVertexOutCaloMuonTracksPerJet = dbe_->get(DirName+"/"+"nInVertexOutCaloMuonTracks"); if(mnInVertexOutCaloMuonTracksPerJet && mnInVertexOutCaloMuonTracksPerJet->getRootObject()) mnInVertexOutCaloMuonTracksPerJet->Fill(muonsInVertexOutCalo.size());
	  for (reco::TrackRefVector::const_iterator iTrack = muonsInVertexInCalo.begin(); iTrack != muonsInVertexInCalo.end(); ++iTrack) {
	    mallMuonTracksPt = dbe_->get(DirName+"/"+"allMuonTracksPt"); if(mallMuonTracksPt && mallMuonTracksPt->getRootObject()) mallMuonTracksPt->Fill((*iTrack)->pt());
	    mallMuonTracksEta = dbe_->get(DirName+"/"+"allMuonTracksEta"); if(mallMuonTracksEta && mallMuonTracksEta->getRootObject()) mallMuonTracksEta->Fill((*iTrack)->eta());
	    mallMuonTracksPhi = dbe_->get(DirName+"/"+"allMuonTracksPhi"); if(mallMuonTracksPhi && mallMuonTracksPhi->getRootObject()) mallMuonTracksPhi->Fill((*iTrack)->phi());
	    mallMuonTracksPtVSEta = dbe_->get(DirName+"/"+"allMuonTracksPtVSEta"); if(mallMuonTracksPtVSEta && mallMuonTracksPtVSEta->getRootObject()) mallMuonTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    mInVertexInCaloMuonTracksPt = dbe_->get(DirName+"/"+"InVertexInCaloMuonTracksPt"); if(mInVertexInCaloMuonTracksPt && mInVertexInCaloMuonTracksPt->getRootObject()) mInVertexInCaloMuonTracksPt->Fill((*iTrack)->pt());
	    mInVertexInCaloMuonTracksEta = dbe_->get(DirName+"/"+"InVertexInCaloMuonTracksEta"); if(mInVertexInCaloMuonTracksEta && mInVertexInCaloMuonTracksEta->getRootObject()) mInVertexInCaloMuonTracksEta->Fill((*iTrack)->eta());
	    mInVertexInCaloMuonTracksPhi = dbe_->get(DirName+"/"+"InVertexInCaloMuonTracksPhi"); if(mInVertexInCaloMuonTracksPhi && mInVertexInCaloMuonTracksPhi->getRootObject()) mInVertexInCaloMuonTracksPhi->Fill((*iTrack)->phi());
	    mInVertexInCaloMuonTracksPtVSEta = dbe_->get(DirName+"/"+"InVertexInCaloMuonTracksPtVSEta"); if(mInVertexInCaloMuonTracksPtVSEta && mInVertexInCaloMuonTracksPtVSEta->getRootObject()) mInVertexInCaloMuonTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    const double trackDirectionJetDR = deltaR(rawJet->eta(),rawJet->phi(),(*iTrack)->eta(),(*iTrack)->phi());
	    mInCaloTrackDirectionJetDRHisto_= dbe_->get(DirName+"/"+"InCaloTrackDirectionJetDR"); if(mInCaloTrackDirectionJetDRHisto_ && mInCaloTrackDirectionJetDRHisto_ ->getRootObject()) if(mInCaloTrackDirectionJetDRHisto_)mInCaloTrackDirectionJetDRHisto_->Fill(trackDirectionJetDR);
	    math::XYZPoint point =trackPropagator_->impactPoint(**iTrack);
	    const double impactPointJetDR = deltaR(rawJet->eta(),rawJet->phi(), point.Eta(),point.Phi());
	     mInVertexTrackImpactPointJetDRHisto_= dbe_->get(DirName+"/"+"InVertexTrackImpactPointJetDR"); if( mInVertexTrackImpactPointJetDRHisto_ &&  mInVertexTrackImpactPointJetDRHisto_ ->getRootObject()) mInVertexTrackImpactPointJetDRHisto_->Fill(impactPointJetDR);
	  }
	  for (reco::TrackRefVector::const_iterator iTrack = muonsInVertexOutCalo.begin(); iTrack != muonsInVertexOutCalo.end(); ++iTrack) {
	    mallMuonTracksPt = dbe_->get(DirName+"/"+"allMuonTracksPt"); if(mallMuonTracksPt && mallMuonTracksPt->getRootObject()) mallMuonTracksPt->Fill((*iTrack)->pt());
	    mallMuonTracksEta = dbe_->get(DirName+"/"+"allMuonTracksEta"); if(mallMuonTracksEta && mallMuonTracksEta->getRootObject()) mallMuonTracksEta->Fill((*iTrack)->eta());
	    mallMuonTracksPhi = dbe_->get(DirName+"/"+"allMuonTracksPhi"); if(mallMuonTracksPhi && mallMuonTracksPhi->getRootObject()) mallMuonTracksPhi->Fill((*iTrack)->phi());
	    mallMuonTracksPtVSEta = dbe_->get(DirName+"/"+"allMuonTracksPtVSEta"); if(mallMuonTracksPtVSEta && mallMuonTracksPtVSEta->getRootObject()) mallMuonTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    mInVertexOutCaloMuonTracksPt = dbe_->get(DirName+"/"+"InVertexOutCaloMuonTracksPt"); if(mInVertexOutCaloMuonTracksPt && mInVertexOutCaloMuonTracksPt->getRootObject()) mInVertexOutCaloMuonTracksPt->Fill((*iTrack)->pt());
	    mInVertexOutCaloMuonTracksEta = dbe_->get(DirName+"/"+"InVertexOutCaloMuonTracksEta"); if(mInVertexOutCaloMuonTracksEta && mInVertexOutCaloMuonTracksEta->getRootObject()) mInVertexOutCaloMuonTracksEta->Fill((*iTrack)->eta());
	    mInVertexOutCaloMuonTracksPhi = dbe_->get(DirName+"/"+"InVertexOutCaloMuonTracksPhi"); if(mInVertexOutCaloMuonTracksPhi && mInVertexOutCaloMuonTracksPhi->getRootObject()) mInVertexOutCaloMuonTracksPhi->Fill((*iTrack)->phi());
	    mInVertexOutCaloMuonTracksPtVSEta = dbe_->get(DirName+"/"+"InVertexOutCaloMuonTracksPtVSEta"); if(mInVertexOutCaloMuonTracksPtVSEta && mInVertexOutCaloMuonTracksPtVSEta->getRootObject()) mInVertexOutCaloMuonTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    const double trackDirectionJetDR = deltaR(rawJet->eta(),rawJet->phi(),(*iTrack)->eta(),(*iTrack)->phi());
	    mOutCaloTrackDirectionJetDRHisto_= dbe_->get(DirName+"/"+"OutCaloTrackDirectionJetDR"); if(mOutCaloTrackDirectionJetDRHisto_ && mOutCaloTrackDirectionJetDRHisto_ ->getRootObject()) if(mOutCaloTrackDirectionJetDRHisto_)mOutCaloTrackDirectionJetDRHisto_->Fill(trackDirectionJetDR);
	    math::XYZPoint point =trackPropagator_->impactPoint(**iTrack);
	    const double impactPointJetDR = deltaR(rawJet->eta(),rawJet->phi(), point.Eta(),point.Phi());
	     mInVertexTrackImpactPointJetDRHisto_= dbe_->get(DirName+"/"+"InVertexTrackImpactPointJetDR"); if( mInVertexTrackImpactPointJetDRHisto_ &&  mInVertexTrackImpactPointJetDRHisto_ ->getRootObject()) mInVertexTrackImpactPointJetDRHisto_->Fill(impactPointJetDR);
	  }
	  for (reco::TrackRefVector::const_iterator iTrack = muonsOutVertexInCalo.begin(); iTrack != muonsOutVertexInCalo.end(); ++iTrack) {
	    mallMuonTracksPt = dbe_->get(DirName+"/"+"allMuonTracksPt"); if(mallMuonTracksPt && mallMuonTracksPt->getRootObject()) mallMuonTracksPt->Fill((*iTrack)->pt());
	    mallMuonTracksEta = dbe_->get(DirName+"/"+"allMuonTracksEta"); if(mallMuonTracksEta && mallMuonTracksEta->getRootObject()) mallMuonTracksEta->Fill((*iTrack)->eta());
	    mallMuonTracksPhi = dbe_->get(DirName+"/"+"allMuonTracksPhi"); if(mallMuonTracksPhi && mallMuonTracksPhi->getRootObject()) mallMuonTracksPhi->Fill((*iTrack)->phi());
	    mallMuonTracksPtVSEta = dbe_->get(DirName+"/"+"allMuonTracksPtVSEta"); if(mallMuonTracksPtVSEta && mallMuonTracksPtVSEta->getRootObject()) mallMuonTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    mOutVertexInCaloMuonTracksPt = dbe_->get(DirName+"/"+"OutVertexInCaloMuonTracksPt"); if(mOutVertexInCaloMuonTracksPt && mOutVertexInCaloMuonTracksPt->getRootObject()) mOutVertexInCaloMuonTracksPt->Fill((*iTrack)->pt());
	    mOutVertexInCaloMuonTracksEta = dbe_->get(DirName+"/"+"OutVertexInCaloMuonTracksEta"); if(mOutVertexInCaloMuonTracksEta && mOutVertexInCaloMuonTracksEta->getRootObject()) mOutVertexInCaloMuonTracksEta->Fill((*iTrack)->eta());
	    mOutVertexInCaloMuonTracksPhi = dbe_->get(DirName+"/"+"OutVertexInCaloMuonTracksPhi"); if(mOutVertexInCaloMuonTracksPhi && mOutVertexInCaloMuonTracksPhi->getRootObject()) mOutVertexInCaloMuonTracksPhi->Fill((*iTrack)->phi());
	    mOutVertexInCaloMuonTracksPtVSEta = dbe_->get(DirName+"/"+"OutVertexInCaloMuonTracksPtVSEta"); if(mOutVertexInCaloMuonTracksPtVSEta && mOutVertexInCaloMuonTracksPtVSEta->getRootObject()) mOutVertexInCaloMuonTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    const double trackDirectionJetDR = deltaR(rawJet->eta(),rawJet->phi(),(*iTrack)->eta(),(*iTrack)->phi());
	    mInCaloTrackDirectionJetDRHisto_= dbe_->get(DirName+"/"+"InCaloTrackDirectionJetDR"); if(mInCaloTrackDirectionJetDRHisto_ && mInCaloTrackDirectionJetDRHisto_ ->getRootObject()) if(mInCaloTrackDirectionJetDRHisto_)mInCaloTrackDirectionJetDRHisto_->Fill(trackDirectionJetDR);
	    math::XYZPoint point =trackPropagator_->impactPoint(**iTrack);
	    const double impactPointJetDR = deltaR(rawJet->eta(),rawJet->phi(), point.Eta(),point.Phi());
	     mOutVertexTrackImpactPointJetDRHisto_= dbe_->get(DirName+"/"+"OutVertexTrackImpactPointJetDR"); if( mOutVertexTrackImpactPointJetDRHisto_ &&  mOutVertexTrackImpactPointJetDRHisto_ ->getRootObject()) mOutVertexTrackImpactPointJetDRHisto_->Fill(impactPointJetDR);
	  }
	  //electron track histos
	  mnallElectronTracksPerJet = dbe_->get(DirName+"/"+"nallElectronTracks"); if(mnallElectronTracksPerJet && mnallElectronTracksPerJet->getRootObject()) mnallElectronTracksPerJet->Fill(electronsInVertexInCalo.size()+electronsInVertexOutCalo.size()+electronsOutVertexInCalo.size());
	  mnInVertexInCaloElectronTracksPerJet = dbe_->get(DirName+"/"+"nInVertexInCaloElectronTracks"); if(mnInVertexInCaloElectronTracksPerJet && mnInVertexInCaloElectronTracksPerJet->getRootObject()) mnInVertexInCaloElectronTracksPerJet->Fill(electronsInVertexInCalo.size());
	  mnOutVertexInCaloElectronTracksPerJet = dbe_->get(DirName+"/"+"nOutVertexInCaloElectronTracks"); if(mnOutVertexInCaloElectronTracksPerJet && mnOutVertexInCaloElectronTracksPerJet->getRootObject()) mnOutVertexInCaloElectronTracksPerJet->Fill(electronsOutVertexInCalo.size());
	  mnInVertexOutCaloElectronTracksPerJet = dbe_->get(DirName+"/"+"nInVertexOutCaloElectronTracks"); if(mnInVertexOutCaloElectronTracksPerJet && mnInVertexOutCaloElectronTracksPerJet->getRootObject()) mnInVertexOutCaloElectronTracksPerJet->Fill(electronsInVertexOutCalo.size());
	  for (reco::TrackRefVector::const_iterator iTrack = electronsInVertexInCalo.begin(); iTrack != electronsInVertexInCalo.end(); ++iTrack) {
	    mallElectronTracksPt = dbe_->get(DirName+"/"+"allElectronTracksPt"); if(mallElectronTracksPt && mallElectronTracksPt->getRootObject()) mallElectronTracksPt->Fill((*iTrack)->pt());
	    mallElectronTracksEta = dbe_->get(DirName+"/"+"allElectronTracksEta"); if(mallElectronTracksEta && mallElectronTracksPhi->getRootObject()) mallElectronTracksEta->Fill((*iTrack)->eta());
	    mallElectronTracksPhi = dbe_->get(DirName+"/"+"allElectronTracksPhi"); if(mallElectronTracksPhi && mallElectronTracksEta->getRootObject()) mallElectronTracksPhi->Fill((*iTrack)->phi());
	    mallElectronTracksPtVSEta = dbe_->get(DirName+"/"+"allElectronTracksPtVSEta"); if(mallElectronTracksPtVSEta && mallElectronTracksPtVSEta->getRootObject()) mallElectronTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    mInVertexInCaloElectronTracksPt = dbe_->get(DirName+"/"+"InVertexInCaloElectronTracksPt"); if(mInVertexInCaloElectronTracksPt && mInVertexInCaloElectronTracksPt->getRootObject()) mInVertexInCaloElectronTracksPt->Fill((*iTrack)->pt());
	    mInVertexInCaloElectronTracksEta = dbe_->get(DirName+"/"+"InVertexInCaloElectronTracksEta"); if(mInVertexInCaloElectronTracksEta && mInVertexInCaloElectronTracksEta->getRootObject()) mInVertexInCaloElectronTracksEta->Fill((*iTrack)->eta());
	    mInVertexInCaloElectronTracksPhi = dbe_->get(DirName+"/"+"InVertexInCaloElectronTracksPhi"); if(mInVertexInCaloElectronTracksPhi && mInVertexInCaloElectronTracksPhi->getRootObject()) mInVertexInCaloElectronTracksPhi->Fill((*iTrack)->phi());
	    mInVertexInCaloElectronTracksPtVSEta = dbe_->get(DirName+"/"+"InVertexInCaloElectronTracksPtVSEta"); if(mInVertexInCaloElectronTracksPtVSEta && mInVertexInCaloElectronTracksPtVSEta->getRootObject()) mInVertexInCaloElectronTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    const double trackDirectionJetDR = deltaR(rawJet->eta(),rawJet->phi(),(*iTrack)->eta(),(*iTrack)->phi());
	    mInCaloTrackDirectionJetDRHisto_= dbe_->get(DirName+"/"+"InCaloTrackDirectionJetDR"); if(mInCaloTrackDirectionJetDRHisto_ && mInCaloTrackDirectionJetDRHisto_ ->getRootObject()) if(mInCaloTrackDirectionJetDRHisto_)mInCaloTrackDirectionJetDRHisto_->Fill(trackDirectionJetDR);
	    math::XYZPoint point =trackPropagator_->impactPoint(**iTrack);
	    const double impactPointJetDR = deltaR(rawJet->eta(),rawJet->phi(), point.Eta(),point.Phi());
	     mInVertexTrackImpactPointJetDRHisto_= dbe_->get(DirName+"/"+"InVertexTrackImpactPointJetDR"); if( mInVertexTrackImpactPointJetDRHisto_ &&  mInVertexTrackImpactPointJetDRHisto_ ->getRootObject()) mInVertexTrackImpactPointJetDRHisto_->Fill(impactPointJetDR);
	  }
	  for (reco::TrackRefVector::const_iterator iTrack = electronsInVertexOutCalo.begin(); iTrack != electronsInVertexOutCalo.end(); ++iTrack) {
	    mallElectronTracksPt = dbe_->get(DirName+"/"+"allElectronTracksPt"); if(mallElectronTracksPt && mallElectronTracksPt->getRootObject()) mallElectronTracksPt->Fill((*iTrack)->pt());
	    mallElectronTracksEta = dbe_->get(DirName+"/"+"allElectronTracksEta"); if(mallElectronTracksEta && mallElectronTracksEta->getRootObject()) mallElectronTracksEta->Fill((*iTrack)->eta());
	    mallElectronTracksPhi = dbe_->get(DirName+"/"+"allElectronTracksPhi"); if(mallElectronTracksPhi && mallElectronTracksPhi->getRootObject()) mallElectronTracksPhi->Fill((*iTrack)->phi());
	    mallElectronTracksPtVSEta = dbe_->get(DirName+"/"+"allElectronTracksPtVSEta"); if(mallElectronTracksPtVSEta && mallElectronTracksPtVSEta->getRootObject()) mallElectronTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    mInVertexOutCaloElectronTracksPt = dbe_->get(DirName+"/"+"InVertexOutCaloElectronTracksPt"); if(mInVertexOutCaloElectronTracksPt && mInVertexOutCaloElectronTracksPt->getRootObject()) mInVertexOutCaloElectronTracksPt->Fill((*iTrack)->pt());
	    mInVertexOutCaloElectronTracksEta = dbe_->get(DirName+"/"+"InVertexOutCaloElectronTracksEta"); if(mInVertexOutCaloElectronTracksEta && mInVertexOutCaloElectronTracksEta->getRootObject()) mInVertexOutCaloElectronTracksEta->Fill((*iTrack)->eta());
	    mInVertexOutCaloElectronTracksPhi = dbe_->get(DirName+"/"+"InVertexOutCaloElectronTracksPhi"); if(mInVertexOutCaloElectronTracksPhi && mInVertexOutCaloElectronTracksPhi->getRootObject()) mInVertexOutCaloElectronTracksPhi->Fill((*iTrack)->phi());
	    mInVertexOutCaloElectronTracksPtVSEta = dbe_->get(DirName+"/"+"InVertexOutCaloElectronTracksPtVSEta"); if(mInVertexOutCaloElectronTracksPtVSEta && mInVertexOutCaloElectronTracksPtVSEta->getRootObject()) mInVertexOutCaloElectronTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    const double trackDirectionJetDR = deltaR(rawJet->eta(),rawJet->phi(),(*iTrack)->eta(),(*iTrack)->phi());
	    mOutCaloTrackDirectionJetDRHisto_= dbe_->get(DirName+"/"+"OutCaloTrackDirectionJetDR"); if(mOutCaloTrackDirectionJetDRHisto_ && mOutCaloTrackDirectionJetDRHisto_ ->getRootObject()) if(mOutCaloTrackDirectionJetDRHisto_)mOutCaloTrackDirectionJetDRHisto_->Fill(trackDirectionJetDR);
	    math::XYZPoint point =trackPropagator_->impactPoint(**iTrack);
	    const double impactPointJetDR = deltaR(rawJet->eta(),rawJet->phi(), point.Eta(),point.Phi());
	     mInVertexTrackImpactPointJetDRHisto_= dbe_->get(DirName+"/"+"InVertexTrackImpactPointJetDR"); if( mInVertexTrackImpactPointJetDRHisto_ &&  mInVertexTrackImpactPointJetDRHisto_ ->getRootObject()) mInVertexTrackImpactPointJetDRHisto_->Fill(impactPointJetDR);
	  }
	  for (reco::TrackRefVector::const_iterator iTrack = electronsOutVertexInCalo.begin(); iTrack != electronsOutVertexInCalo.end(); ++iTrack) {
	    mallElectronTracksPt = dbe_->get(DirName+"/"+"allElectronTracksPt"); if(mallElectronTracksPt && mallElectronTracksPt->getRootObject()) mallElectronTracksPt->Fill((*iTrack)->pt());
	    mallElectronTracksEta = dbe_->get(DirName+"/"+"allElectronTracksEta"); if(mallElectronTracksEta && mallElectronTracksEta->getRootObject()) mallElectronTracksEta->Fill((*iTrack)->eta());
	    mallElectronTracksPhi = dbe_->get(DirName+"/"+"allElectronTracksPhi"); if(mallElectronTracksPhi && mallElectronTracksPhi->getRootObject()) mallElectronTracksPhi->Fill((*iTrack)->phi());
	    mallElectronTracksPtVSEta = dbe_->get(DirName+"/"+"allElectronTracksPtVSEta"); if(mallElectronTracksPtVSEta && mallElectronTracksPtVSEta->getRootObject()) mallElectronTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    mOutVertexInCaloElectronTracksPt = dbe_->get(DirName+"/"+"OutVertexInCaloElectronTracksPt"); if(mOutVertexInCaloElectronTracksPt && mOutVertexInCaloElectronTracksPt->getRootObject()) mOutVertexInCaloElectronTracksPt->Fill((*iTrack)->pt());
	    mOutVertexInCaloElectronTracksEta = dbe_->get(DirName+"/"+"OutVertexInCaloElectronTracksEta"); if(mOutVertexInCaloElectronTracksEta && mOutVertexInCaloElectronTracksEta->getRootObject()) mOutVertexInCaloElectronTracksEta->Fill((*iTrack)->eta());
	    mOutVertexInCaloElectronTracksPhi = dbe_->get(DirName+"/"+"OutVertexInCaloElectronTracksPhi"); if(mOutVertexInCaloElectronTracksPhi && mOutVertexInCaloElectronTracksPhi->getRootObject()) mOutVertexInCaloElectronTracksPhi->Fill((*iTrack)->phi());
	    mOutVertexInCaloElectronTracksPtVSEta = dbe_->get(DirName+"/"+"OutVertexInCaloElectronTracksPtVSEta"); if(mOutVertexInCaloElectronTracksPtVSEta && mOutVertexInCaloElectronTracksPtVSEta->getRootObject()) mOutVertexInCaloElectronTracksPtVSEta->Fill((*iTrack)->eta(),(*iTrack)->pt());
	    const double trackDirectionJetDR = deltaR(rawJet->eta(),rawJet->phi(),(*iTrack)->eta(),(*iTrack)->phi());
	    mInCaloTrackDirectionJetDRHisto_= dbe_->get(DirName+"/"+"InCaloTrackDirectionJetDR"); if(mInCaloTrackDirectionJetDRHisto_ && mInCaloTrackDirectionJetDRHisto_ ->getRootObject()) if(mInCaloTrackDirectionJetDRHisto_)mInCaloTrackDirectionJetDRHisto_->Fill(trackDirectionJetDR);
	    math::XYZPoint point =trackPropagator_->impactPoint(**iTrack);
	    const double impactPointJetDR = deltaR(rawJet->eta(),rawJet->phi(), point.Eta(),point.Phi());
	     mOutVertexTrackImpactPointJetDRHisto_= dbe_->get(DirName+"/"+"OutVertexTrackImpactPointJetDR"); if( mOutVertexTrackImpactPointJetDRHisto_ &&  mOutVertexTrackImpactPointJetDRHisto_ ->getRootObject()) mOutVertexTrackImpactPointJetDRHisto_->Fill(impactPointJetDR);
	  }
	}
      }
      if(isPFJet_){
	jetpassid = pfjetIDFunctor((*pfJets)[ijet]);
	if(jetCleaningFlag_){
	  Thiscleaned = jetpassid;
	}
	if(Thiscleaned && pass_uncorrected){
	  mPt_uncor = dbe_->get(DirName+"/"+"Pt_uncor"); if (mPt_uncor && mPt_uncor->getRootObject()) if (mPt_uncor)   mPt_uncor->Fill ((*pfJets)[ijet].pt());
	  mEta_uncor = dbe_->get(DirName+"/"+"Eta_uncor"); if (mEta_uncor && mEta_uncor->getRootObject()) if (mEta_uncor)  mEta_uncor->Fill ((*pfJets)[ijet].eta());
	  mPhi_uncor = dbe_->get(DirName+"/"+"Phi_uncor"); if (mPhi_uncor && mPhi_uncor->getRootObject()) if (mPhi_uncor)  mPhi_uncor->Fill ((*pfJets)[ijet].phi());
	  if(!isJPTJet_){
	    mConstituents_uncor = dbe_->get(DirName+"/"+"Constituents_uncor"); if (mConstituents_uncor && mConstituents_uncor->getRootObject()) if (mConstituents_uncor) mConstituents_uncor->Fill ((*pfJets)[ijet].nConstituents());
	  }
	}
	if(Thiscleaned && pass_corrected){
	  mHFrac = dbe_->get(DirName+"/"+"HFrac"); if (mHFrac && mHFrac->getRootObject()) mHFrac->Fill ((*pfJets)[ijet].chargedHadronEnergyFraction()+(*pfJets)[ijet].neutralHadronEnergyFraction()+(*pfJets)[ijet].HFHadronEnergyFraction ());
	  mEFrac = dbe_->get(DirName+"/"+"EFrac"); if (mEFrac && mHFrac->getRootObject()) mEFrac->Fill ((*pfJets)[ijet].chargedEmEnergyFraction() +(*pfJets)[ijet].neutralEmEnergyFraction()+(*pfJets)[ijet].HFEMEnergyFraction ());
	  if ((*pfJets)[ijet].pt()<= 50) {
	    mCHFracVSeta_lowPt = dbe_->get(DirName+"/"+"CHFracVSeta_lowPt"); if (mCHFracVSeta_lowPt &&  mCHFracVSeta_lowPt->getRootObject()) mCHFracVSeta_lowPt->Fill((*pfJets)[ijet].eta(),(*pfJets)[ijet].chargedHadronEnergyFraction());
	    mNHFracVSeta_lowPt = dbe_->get(DirName+"/"+"NHFracVSeta_lowPt"); if (mNHFracVSeta_lowPt &&  mNHFracVSeta_lowPt->getRootObject()) mNHFracVSeta_lowPt->Fill((*pfJets)[ijet].eta(),(*pfJets)[ijet].neutralHadronEnergyFraction());
	    mPhFracVSeta_lowPt = dbe_->get(DirName+"/"+"PhFracVSeta_lowPt"); if (mPhFracVSeta_lowPt &&  mPhFracVSeta_lowPt->getRootObject()) mPhFracVSeta_lowPt->Fill((*pfJets)[ijet].eta(),(*pfJets)[ijet].neutralEmEnergyFraction());
	    mElFracVSeta_lowPt = dbe_->get(DirName+"/"+"ElFracVSeta_lowPt"); if (mElFracVSeta_lowPt &&  mElFracVSeta_lowPt->getRootObject()) mElFracVSeta_lowPt->Fill((*pfJets)[ijet].eta(),(*pfJets)[ijet].chargedEmEnergyFraction());
	    mMuFracVSeta_lowPt = dbe_->get(DirName+"/"+"MuFracVSeta_lowPt"); if (mMuFracVSeta_lowPt &&  mMuFracVSeta_lowPt->getRootObject()) mMuFracVSeta_lowPt->Fill((*pfJets)[ijet].eta(),(*pfJets)[ijet].chargedMuEnergyFraction());
	  }
	  if ((*pfJets)[ijet].pt()>50. && (*pfJets)[ijet].pt()<=140.) {
	    mCHFracVSeta_mediumPt = dbe_->get(DirName+"/"+"CHFracVSeta_mediumPt"); if (mCHFracVSeta_mediumPt &&  mCHFracVSeta_mediumPt->getRootObject()) mCHFracVSeta_mediumPt->Fill((*pfJets)[ijet].eta(),(*pfJets)[ijet].chargedHadronEnergyFraction());
	    mNHFracVSeta_mediumPt = dbe_->get(DirName+"/"+"NHFracVSeta_mediumPt"); if (mNHFracVSeta_mediumPt &&  mNHFracVSeta_mediumPt->getRootObject()) mNHFracVSeta_mediumPt->Fill((*pfJets)[ijet].eta(),(*pfJets)[ijet].neutralHadronEnergyFraction());
	    mPhFracVSeta_mediumPt = dbe_->get(DirName+"/"+"PhFracVSeta_mediumPt"); if (mPhFracVSeta_mediumPt &&  mPhFracVSeta_mediumPt->getRootObject()) mPhFracVSeta_mediumPt->Fill((*pfJets)[ijet].eta(),(*pfJets)[ijet].neutralEmEnergyFraction());
	    mElFracVSeta_mediumPt = dbe_->get(DirName+"/"+"ElFracVSeta_mediumPt"); if (mElFracVSeta_mediumPt &&  mElFracVSeta_mediumPt->getRootObject()) mElFracVSeta_mediumPt->Fill((*pfJets)[ijet].eta(),(*pfJets)[ijet].chargedEmEnergyFraction());
	    mMuFracVSeta_mediumPt = dbe_->get(DirName+"/"+"MuFracVSeta_mediumPt"); if (mMuFracVSeta_mediumPt &&  mMuFracVSeta_mediumPt->getRootObject()) mMuFracVSeta_mediumPt->Fill((*pfJets)[ijet].eta(),(*pfJets)[ijet].chargedMuEnergyFraction());
	  }
	  if ((*pfJets)[ijet].pt()>140.) {
	    mCHFracVSeta_highPt = dbe_->get(DirName+"/"+"CHFracVSeta_highPt"); if (mCHFracVSeta_highPt &&  mCHFracVSeta_highPt->getRootObject()) mCHFracVSeta_highPt->Fill((*pfJets)[ijet].eta(),(*pfJets)[ijet].chargedHadronEnergyFraction());
	    mNHFracVSeta_highPt = dbe_->get(DirName+"/"+"NHFracVSeta_highPt"); if (mNHFracVSeta_highPt &&  mNHFracVSeta_highPt->getRootObject()) mNHFracVSeta_highPt->Fill((*pfJets)[ijet].eta(),(*pfJets)[ijet].neutralHadronEnergyFraction());
	    mPhFracVSeta_highPt = dbe_->get(DirName+"/"+"PhFracVSeta_highPt"); if (mPhFracVSeta_highPt &&  mPhFracVSeta_highPt->getRootObject()) mPhFracVSeta_highPt->Fill((*pfJets)[ijet].eta(),(*pfJets)[ijet].neutralEmEnergyFraction());
	    mElFracVSeta_highPt = dbe_->get(DirName+"/"+"ElFracVSeta_highPt"); if (mElFracVSeta_highPt &&  mElFracVSeta_highPt->getRootObject()) mElFracVSeta_highPt->Fill((*pfJets)[ijet].eta(),(*pfJets)[ijet].chargedEmEnergyFraction());
	    mMuFracVSeta_highPt = dbe_->get(DirName+"/"+"MuFracVSeta_highPt"); if (mMuFracVSeta_highPt &&  mMuFracVSeta_highPt->getRootObject()) mMuFracVSeta_highPt->Fill((*pfJets)[ijet].eta(),(*pfJets)[ijet].chargedMuEnergyFraction());
	  }
	  if (fabs((*pfJets)[ijet].eta()) <= 1.3) {
	     mHFrac_Barrel = dbe_->get(DirName+"/"+"HFrac_Barrel"); if (mHFrac_Barrel && mHFrac_Barrel->getRootObject())   mHFrac_Barrel->Fill((*pfJets)[ijet].chargedHadronEnergyFraction() + (*pfJets)[ijet].neutralHadronEnergyFraction() );
	     mEFrac_Barrel = dbe_->get(DirName+"/"+"EFrac_Barrel"); if (mEFrac_Barrel && mEFrac_Barrel->getRootObject())   mEFrac_Barrel->Fill ((*pfJets)[ijet].chargedEmEnergyFraction() + (*pfJets)[ijet].neutralEmEnergyFraction());
	    //fractions for barrel
	    if ((*pfJets)[ijet].pt()<=50.) {
	      mCHFrac_lowPt_Barrel = dbe_->get(DirName+"/"+"CHFrac_lowPt_Barrel"); if (mCHFrac_lowPt_Barrel &&  mCHFrac_lowPt_Barrel->getRootObject()) mCHFrac_lowPt_Barrel->Fill((*pfJets)[ijet].chargedHadronEnergyFraction());
	      mNHFrac_lowPt_Barrel = dbe_->get(DirName+"/"+"NHFrac_lowPt_Barrel"); if (mNHFrac_lowPt_Barrel &&  mNHFrac_lowPt_Barrel->getRootObject()) mNHFrac_lowPt_Barrel->Fill((*pfJets)[ijet].neutralHadronEnergyFraction());
	      mPhFrac_lowPt_Barrel = dbe_->get(DirName+"/"+"PhFrac_lowPt_Barrel"); if (mPhFrac_lowPt_Barrel &&  mPhFrac_lowPt_Barrel->getRootObject()) mPhFrac_lowPt_Barrel->Fill((*pfJets)[ijet].neutralEmEnergyFraction());
	      mElFrac_lowPt_Barrel = dbe_->get(DirName+"/"+"ElFrac_lowPt_Barrel"); if (mElFrac_lowPt_Barrel &&  mElFrac_lowPt_Barrel->getRootObject()) mElFrac_lowPt_Barrel->Fill((*pfJets)[ijet].chargedEmEnergyFraction());
	      mMuFrac_lowPt_Barrel = dbe_->get(DirName+"/"+"MuFrac_lowPt_Barrel"); if (mMuFrac_lowPt_Barrel &&  mMuFrac_lowPt_Barrel->getRootObject()) mMuFrac_lowPt_Barrel->Fill((*pfJets)[ijet].chargedMuEnergyFraction());
  	      mCHEn_lowPt_Barrel = dbe_->get(DirName+"/"+"CHEn_lowPt_Barrel"); if (mCHEn_lowPt_Barrel &&  mCHEn_lowPt_Barrel->getRootObject()) mCHEn_lowPt_Barrel->Fill((*pfJets)[ijet].chargedHadronEnergy());
	      mNHEn_lowPt_Barrel = dbe_->get(DirName+"/"+"NHEn_lowPt_Barrel"); if (mNHEn_lowPt_Barrel &&  mNHEn_lowPt_Barrel->getRootObject()) mNHEn_lowPt_Barrel->Fill((*pfJets)[ijet].neutralHadronEnergy());
	      mPhEn_lowPt_Barrel = dbe_->get(DirName+"/"+"PhEn_lowPt_Barrel"); if (mPhEn_lowPt_Barrel &&  mPhEn_lowPt_Barrel->getRootObject()) mPhEn_lowPt_Barrel->Fill((*pfJets)[ijet].neutralEmEnergy());
	      mElEn_lowPt_Barrel = dbe_->get(DirName+"/"+"ElEn_lowPt_Barrel"); if (mElEn_lowPt_Barrel &&  mElEn_lowPt_Barrel->getRootObject()) mElEn_lowPt_Barrel->Fill((*pfJets)[ijet].chargedEmEnergy());
	      mMuEn_lowPt_Barrel = dbe_->get(DirName+"/"+"MuEn_lowPt_Barrel"); if (mMuEn_lowPt_Barrel &&  mMuEn_lowPt_Barrel->getRootObject()) mMuEn_lowPt_Barrel->Fill((*pfJets)[ijet].chargedMuEnergy());
	      mChMultiplicity_lowPt_Barrel = dbe_->get(DirName+"/"+"ChMultiplicity_lowPt_Barrel"); if(mChMultiplicity_lowPt_Barrel && mChMultiplicity_lowPt_Barrel->getRootObject())  mChMultiplicity_lowPt_Barrel->Fill((*pfJets)[ijet].chargedMultiplicity());
	      mNeutMultiplicity_lowPt_Barrel = dbe_->get(DirName+"/"+"NeutMultiplicity_lowPt_Barrel"); if(mNeutMultiplicity_lowPt_Barrel && mNeutMultiplicity_lowPt_Barrel->getRootObject())  mNeutMultiplicity_lowPt_Barrel->Fill((*pfJets)[ijet].neutralMultiplicity());
	      mMuMultiplicity_lowPt_Barrel = dbe_->get(DirName+"/"+"MuMultiplicity_lowPt_Barrel"); if(mMuMultiplicity_lowPt_Barrel && mMuMultiplicity_lowPt_Barrel->getRootObject())  mMuMultiplicity_lowPt_Barrel->Fill((*pfJets)[ijet].muonMultiplicity());
	    }
	    if ((*pfJets)[ijet].pt()>50. && (*pfJets)[ijet].pt()<=140.) {
	      mCHFrac_mediumPt_Barrel = dbe_->get(DirName+"/"+"CHFrac_mediumPt_Barrel"); if (mCHFrac_mediumPt_Barrel &&  mCHFrac_mediumPt_Barrel->getRootObject()) mCHFrac_mediumPt_Barrel->Fill((*pfJets)[ijet].chargedHadronEnergyFraction());
	      mNHFrac_mediumPt_Barrel = dbe_->get(DirName+"/"+"NHFrac_mediumPt_Barrel"); if (mNHFrac_mediumPt_Barrel &&  mNHFrac_mediumPt_Barrel->getRootObject()) mNHFrac_mediumPt_Barrel->Fill((*pfJets)[ijet].neutralHadronEnergyFraction());
	      mPhFrac_mediumPt_Barrel = dbe_->get(DirName+"/"+"PhFrac_mediumPt_Barrel"); if (mPhFrac_mediumPt_Barrel &&  mPhFrac_mediumPt_Barrel->getRootObject()) mPhFrac_mediumPt_Barrel->Fill((*pfJets)[ijet].neutralEmEnergyFraction());
	      mElFrac_mediumPt_Barrel = dbe_->get(DirName+"/"+"ElFrac_mediumPt_Barrel"); if (mElFrac_mediumPt_Barrel &&  mElFrac_mediumPt_Barrel->getRootObject()) mElFrac_mediumPt_Barrel->Fill((*pfJets)[ijet].chargedEmEnergyFraction());
	      mMuFrac_mediumPt_Barrel = dbe_->get(DirName+"/"+"MuFrac_mediumPt_Barrel"); if (mMuFrac_mediumPt_Barrel &&  mMuFrac_mediumPt_Barrel->getRootObject()) mMuFrac_mediumPt_Barrel->Fill((*pfJets)[ijet].chargedMuEnergyFraction());
  	      mCHEn_mediumPt_Barrel = dbe_->get(DirName+"/"+"CHEn_mediumPt_Barrel"); if (mCHEn_mediumPt_Barrel &&  mCHEn_mediumPt_Barrel->getRootObject()) mCHEn_mediumPt_Barrel->Fill((*pfJets)[ijet].chargedHadronEnergy());
	      mNHEn_mediumPt_Barrel = dbe_->get(DirName+"/"+"NHEn_mediumPt_Barrel"); if (mNHEn_mediumPt_Barrel &&  mNHEn_mediumPt_Barrel->getRootObject()) mNHEn_mediumPt_Barrel->Fill((*pfJets)[ijet].neutralHadronEnergy());
	      mPhEn_mediumPt_Barrel = dbe_->get(DirName+"/"+"PhEn_mediumPt_Barrel"); if (mPhEn_mediumPt_Barrel &&  mPhEn_mediumPt_Barrel->getRootObject()) mPhEn_mediumPt_Barrel->Fill((*pfJets)[ijet].neutralEmEnergy());
	      mElEn_mediumPt_Barrel = dbe_->get(DirName+"/"+"ElEn_mediumPt_Barrel"); if (mElEn_mediumPt_Barrel &&  mElEn_mediumPt_Barrel->getRootObject()) mElEn_mediumPt_Barrel->Fill((*pfJets)[ijet].chargedEmEnergy());
	      mMuEn_mediumPt_Barrel = dbe_->get(DirName+"/"+"MuEn_mediumPt_Barrel"); if (mMuEn_mediumPt_Barrel &&  mMuEn_mediumPt_Barrel->getRootObject()) mMuEn_mediumPt_Barrel->Fill((*pfJets)[ijet].chargedMuEnergy());
	      mChMultiplicity_mediumPt_Barrel = dbe_->get(DirName+"/"+"ChMultiplicity_mediumPt_Barrel"); if(mChMultiplicity_mediumPt_Barrel && mChMultiplicity_mediumPt_Barrel->getRootObject())  mChMultiplicity_mediumPt_Barrel->Fill((*pfJets)[ijet].chargedMultiplicity());
	      mNeutMultiplicity_mediumPt_Barrel = dbe_->get(DirName+"/"+"NeutMultiplicity_mediumPt_Barrel"); if(mNeutMultiplicity_mediumPt_Barrel && mNeutMultiplicity_mediumPt_Barrel->getRootObject())  mNeutMultiplicity_mediumPt_Barrel->Fill((*pfJets)[ijet].neutralMultiplicity());
	      mMuMultiplicity_mediumPt_Barrel = dbe_->get(DirName+"/"+"MuMultiplicity_mediumPt_Barrel"); if(mMuMultiplicity_mediumPt_Barrel && mMuMultiplicity_mediumPt_Barrel->getRootObject())  mMuMultiplicity_mediumPt_Barrel->Fill((*pfJets)[ijet].muonMultiplicity());
	    }
	    if ((*pfJets)[ijet].pt()>140.) {
	      mCHFrac_highPt_Barrel = dbe_->get(DirName+"/"+"CHFrac_highPt_Barrel"); if (mCHFrac_highPt_Barrel &&  mCHFrac_highPt_Barrel->getRootObject()) mCHFrac_highPt_Barrel->Fill((*pfJets)[ijet].chargedHadronEnergyFraction());
	      mNHFrac_highPt_Barrel = dbe_->get(DirName+"/"+"NHFrac_highPt_Barrel"); if (mNHFrac_highPt_Barrel &&  mNHFrac_highPt_Barrel->getRootObject()) mNHFrac_highPt_Barrel->Fill((*pfJets)[ijet].neutralHadronEnergyFraction());
	      mPhFrac_highPt_Barrel = dbe_->get(DirName+"/"+"PhFrac_highPt_Barrel"); if (mPhFrac_highPt_Barrel &&  mPhFrac_highPt_Barrel->getRootObject()) mPhFrac_highPt_Barrel->Fill((*pfJets)[ijet].neutralEmEnergyFraction());
	      mElFrac_highPt_Barrel = dbe_->get(DirName+"/"+"ElFrac_highPt_Barrel"); if (mElFrac_highPt_Barrel &&  mElFrac_highPt_Barrel->getRootObject()) mElFrac_highPt_Barrel->Fill((*pfJets)[ijet].chargedEmEnergyFraction());
	      mMuFrac_highPt_Barrel = dbe_->get(DirName+"/"+"MuFrac_highPt_Barrel"); if (mMuFrac_highPt_Barrel &&  mMuFrac_highPt_Barrel->getRootObject()) mMuFrac_highPt_Barrel->Fill((*pfJets)[ijet].chargedMuEnergyFraction());
  	      mCHEn_highPt_Barrel = dbe_->get(DirName+"/"+"CHEn_highPt_Barrel"); if (mCHEn_highPt_Barrel &&  mCHEn_highPt_Barrel->getRootObject()) mCHEn_highPt_Barrel->Fill((*pfJets)[ijet].chargedHadronEnergy());
	      mNHEn_highPt_Barrel = dbe_->get(DirName+"/"+"NHEn_highPt_Barrel"); if (mNHEn_highPt_Barrel &&  mNHEn_highPt_Barrel->getRootObject()) mNHEn_highPt_Barrel->Fill((*pfJets)[ijet].neutralHadronEnergy());
	      mPhEn_highPt_Barrel = dbe_->get(DirName+"/"+"PhEn_highPt_Barrel"); if (mPhEn_highPt_Barrel &&  mPhEn_highPt_Barrel->getRootObject()) mPhEn_highPt_Barrel->Fill((*pfJets)[ijet].neutralEmEnergy());
	      mElEn_highPt_Barrel = dbe_->get(DirName+"/"+"ElEn_highPt_Barrel"); if (mElEn_highPt_Barrel &&  mElEn_highPt_Barrel->getRootObject()) mElEn_highPt_Barrel->Fill((*pfJets)[ijet].chargedEmEnergy());
	      mMuEn_highPt_Barrel = dbe_->get(DirName+"/"+"MuEn_highPt_Barrel"); if (mMuEn_highPt_Barrel &&  mMuEn_highPt_Barrel->getRootObject()) mMuEn_highPt_Barrel->Fill((*pfJets)[ijet].chargedMuEnergy());
	      mChMultiplicity_highPt_Barrel = dbe_->get(DirName+"/"+"ChMultiplicity_highPt_Barrel"); if(mChMultiplicity_highPt_Barrel && mChMultiplicity_highPt_Barrel->getRootObject())  mChMultiplicity_highPt_Barrel->Fill((*pfJets)[ijet].chargedMultiplicity());
	      mNeutMultiplicity_highPt_Barrel = dbe_->get(DirName+"/"+"NeutMultiplicity_highPt_Barrel"); if(mNeutMultiplicity_highPt_Barrel && mNeutMultiplicity_highPt_Barrel->getRootObject())  mNeutMultiplicity_highPt_Barrel->Fill((*pfJets)[ijet].neutralMultiplicity());
	      mMuMultiplicity_highPt_Barrel = dbe_->get(DirName+"/"+"MuMultiplicity_highPt_Barrel"); if(mMuMultiplicity_highPt_Barrel && mMuMultiplicity_highPt_Barrel->getRootObject())  mMuMultiplicity_highPt_Barrel->Fill((*pfJets)[ijet].muonMultiplicity());
	    }
	    mCHFracVSpT_Barrel = dbe_->get(DirName+"/"+"CHFracVSpT_Barrel"); if(mCHFracVSpT_Barrel && mCHFracVSpT_Barrel->getRootObject()) mCHFracVSpT_Barrel->Fill((*pfJets)[ijet].pt(),(*pfJets)[ijet].chargedHadronEnergyFraction());
	    mNHFracVSpT_Barrel = dbe_->get(DirName+"/"+"NHFracVSpT_Barrel");if (mNHFracVSpT_Barrel && mNHFracVSpT_Barrel->getRootObject()) mNHFracVSpT_Barrel->Fill((*pfJets)[ijet].pt(),(*pfJets)[ijet].neutralHadronEnergyFraction());
	    mPhFracVSpT_Barrel = dbe_->get(DirName+"/"+"PhFracVSpT_Barrel");if (mPhFracVSpT_Barrel && mPhFracVSpT_Barrel->getRootObject()) mPhFracVSpT_Barrel->Fill((*pfJets)[ijet].pt(),(*pfJets)[ijet].neutralEmEnergyFraction());
	    mElFracVSpT_Barrel = dbe_->get(DirName+"/"+"ElFracVSpT_Barrel");if (mElFracVSpT_Barrel && mElFracVSpT_Barrel->getRootObject()) mElFracVSpT_Barrel->Fill((*pfJets)[ijet].pt(),(*pfJets)[ijet].chargedEmEnergyFraction());
	    mMuFracVSpT_Barrel = dbe_->get(DirName+"/"+"MuFracVSpT_Barrel");if (mMuFracVSpT_Barrel && mMuFracVSpT_Barrel->getRootObject()) mMuFracVSpT_Barrel->Fill((*pfJets)[ijet].pt(),(*pfJets)[ijet].chargedMuEnergyFraction());
	  }else if(fabs((*pfJets)[ijet].eta()) <= 3) {
	    mHFrac_EndCap = dbe_->get(DirName+"/"+"HFrac_EndCap"); if (mHFrac_EndCap && mHFrac_EndCap->getRootObject())   mHFrac_EndCap->Fill((*pfJets)[ijet].chargedHadronEnergyFraction() + (*pfJets)[ijet].neutralHadronEnergyFraction()+(*pfJets)[ijet].HFHadronEnergyFraction ());
	    mEFrac_EndCap = dbe_->get(DirName+"/"+"EFrac_EndCap"); if (mEFrac_EndCap && mEFrac_EndCap->getRootObject())    mEFrac_EndCap->Fill ((*pfJets)[ijet].chargedEmEnergyFraction() + (*pfJets)[ijet].neutralEmEnergyFraction()+(*pfJets)[ijet].HFEMEnergyFraction ());
	    //fractions for endcap
	    if ((*pfJets)[ijet].pt()<=50.) {
	      mCHFrac_lowPt_EndCap = dbe_->get(DirName+"/"+"CHFrac_lowPt_EndCap"); if (mCHFrac_lowPt_EndCap &&  mCHFrac_lowPt_EndCap->getRootObject()) mCHFrac_lowPt_EndCap->Fill((*pfJets)[ijet].chargedHadronEnergyFraction());
	      mNHFrac_lowPt_EndCap = dbe_->get(DirName+"/"+"NHFrac_lowPt_EndCap"); if (mNHFrac_lowPt_EndCap &&  mNHFrac_lowPt_EndCap->getRootObject()) mNHFrac_lowPt_EndCap->Fill((*pfJets)[ijet].neutralHadronEnergyFraction());
	      mPhFrac_lowPt_EndCap = dbe_->get(DirName+"/"+"PhFrac_lowPt_EndCap"); if (mPhFrac_lowPt_EndCap &&  mPhFrac_lowPt_EndCap->getRootObject()) mPhFrac_lowPt_EndCap->Fill((*pfJets)[ijet].neutralEmEnergyFraction());
	      mElFrac_lowPt_EndCap = dbe_->get(DirName+"/"+"ElFrac_lowPt_EndCap"); if (mElFrac_lowPt_EndCap &&  mElFrac_lowPt_EndCap->getRootObject()) mElFrac_lowPt_EndCap->Fill((*pfJets)[ijet].chargedEmEnergyFraction());
	      mMuFrac_lowPt_EndCap = dbe_->get(DirName+"/"+"MuFrac_lowPt_EndCap"); if (mMuFrac_lowPt_EndCap &&  mMuFrac_lowPt_EndCap->getRootObject()) mMuFrac_lowPt_EndCap->Fill((*pfJets)[ijet].chargedMuEnergyFraction());
  	      mCHEn_lowPt_EndCap = dbe_->get(DirName+"/"+"CHEn_lowPt_EndCap"); if (mCHEn_lowPt_EndCap &&  mCHEn_lowPt_EndCap->getRootObject()) mCHEn_lowPt_EndCap->Fill((*pfJets)[ijet].chargedHadronEnergy());
	      mNHEn_lowPt_EndCap = dbe_->get(DirName+"/"+"NHEn_lowPt_EndCap"); if (mNHEn_lowPt_EndCap &&  mNHEn_lowPt_EndCap->getRootObject()) mNHEn_lowPt_EndCap->Fill((*pfJets)[ijet].neutralHadronEnergy());
	      mPhEn_lowPt_EndCap = dbe_->get(DirName+"/"+"PhEn_lowPt_EndCap"); if (mPhEn_lowPt_EndCap &&  mPhEn_lowPt_EndCap->getRootObject()) mPhEn_lowPt_EndCap->Fill((*pfJets)[ijet].neutralEmEnergy());
	      mElEn_lowPt_EndCap = dbe_->get(DirName+"/"+"ElEn_lowPt_EndCap"); if (mElEn_lowPt_EndCap &&  mElEn_lowPt_EndCap->getRootObject()) mElEn_lowPt_EndCap->Fill((*pfJets)[ijet].chargedEmEnergy());
	      mMuEn_lowPt_EndCap = dbe_->get(DirName+"/"+"MuEn_lowPt_EndCap"); if (mMuEn_lowPt_EndCap &&  mMuEn_lowPt_EndCap->getRootObject()) mMuEn_lowPt_EndCap->Fill((*pfJets)[ijet].chargedMuEnergy());
	      mChMultiplicity_lowPt_EndCap = dbe_->get(DirName+"/"+"ChMultiplicity_lowPt_EndCap"); if(mChMultiplicity_lowPt_EndCap && mChMultiplicity_lowPt_EndCap->getRootObject())  mChMultiplicity_lowPt_EndCap->Fill((*pfJets)[ijet].chargedMultiplicity());
	      mNeutMultiplicity_lowPt_EndCap = dbe_->get(DirName+"/"+"NeutMultiplicity_lowPt_EndCap"); if(mNeutMultiplicity_lowPt_EndCap && mNeutMultiplicity_lowPt_EndCap->getRootObject())  mNeutMultiplicity_lowPt_EndCap->Fill((*pfJets)[ijet].neutralMultiplicity());
	      mMuMultiplicity_lowPt_EndCap = dbe_->get(DirName+"/"+"MuMultiplicity_lowPt_EndCap"); if(mMuMultiplicity_lowPt_EndCap && mMuMultiplicity_lowPt_EndCap->getRootObject())  mMuMultiplicity_lowPt_EndCap->Fill((*pfJets)[ijet].muonMultiplicity());
	    }
	    if ((*pfJets)[ijet].pt()>50. && (*pfJets)[ijet].pt()<=140.) {
	      mCHFrac_mediumPt_EndCap = dbe_->get(DirName+"/"+"CHFrac_mediumPt_EndCap"); if (mCHFrac_mediumPt_EndCap &&  mCHFrac_mediumPt_EndCap->getRootObject()) mCHFrac_mediumPt_EndCap->Fill((*pfJets)[ijet].chargedHadronEnergyFraction());
	      mNHFrac_mediumPt_EndCap = dbe_->get(DirName+"/"+"NHFrac_mediumPt_EndCap"); if (mNHFrac_mediumPt_EndCap &&  mNHFrac_mediumPt_EndCap->getRootObject()) mNHFrac_mediumPt_EndCap->Fill((*pfJets)[ijet].neutralHadronEnergyFraction());
	      mPhFrac_mediumPt_EndCap = dbe_->get(DirName+"/"+"PhFrac_mediumPt_EndCap"); if (mPhFrac_mediumPt_EndCap &&  mPhFrac_mediumPt_EndCap->getRootObject()) mPhFrac_mediumPt_EndCap->Fill((*pfJets)[ijet].neutralEmEnergyFraction());
	      mElFrac_mediumPt_EndCap = dbe_->get(DirName+"/"+"ElFrac_mediumPt_EndCap"); if (mElFrac_mediumPt_EndCap &&  mElFrac_mediumPt_EndCap->getRootObject()) mElFrac_mediumPt_EndCap->Fill((*pfJets)[ijet].chargedEmEnergyFraction());
	      mMuFrac_mediumPt_EndCap = dbe_->get(DirName+"/"+"MuFrac_mediumPt_EndCap"); if (mMuFrac_mediumPt_EndCap &&  mMuFrac_mediumPt_EndCap->getRootObject()) mMuFrac_mediumPt_EndCap->Fill((*pfJets)[ijet].chargedMuEnergyFraction());
  	      mCHEn_mediumPt_EndCap = dbe_->get(DirName+"/"+"CHEn_mediumPt_EndCap"); if (mCHEn_mediumPt_EndCap &&  mCHEn_mediumPt_EndCap->getRootObject()) mCHEn_mediumPt_EndCap->Fill((*pfJets)[ijet].chargedHadronEnergy());
	      mNHEn_mediumPt_EndCap = dbe_->get(DirName+"/"+"NHEn_mediumPt_EndCap"); if (mNHEn_mediumPt_EndCap &&  mNHEn_mediumPt_EndCap->getRootObject()) mNHEn_mediumPt_EndCap->Fill((*pfJets)[ijet].neutralHadronEnergy());
	      mPhEn_mediumPt_EndCap = dbe_->get(DirName+"/"+"PhEn_mediumPt_EndCap"); if (mPhEn_mediumPt_EndCap &&  mPhEn_mediumPt_EndCap->getRootObject()) mPhEn_mediumPt_EndCap->Fill((*pfJets)[ijet].neutralEmEnergy());
	      mElEn_mediumPt_EndCap = dbe_->get(DirName+"/"+"ElEn_mediumPt_EndCap"); if (mElEn_mediumPt_EndCap &&  mElEn_mediumPt_EndCap->getRootObject()) mElEn_mediumPt_EndCap->Fill((*pfJets)[ijet].chargedEmEnergy());
	      mMuEn_mediumPt_EndCap = dbe_->get(DirName+"/"+"MuEn_mediumPt_EndCap"); if (mMuEn_mediumPt_EndCap &&  mMuEn_mediumPt_EndCap->getRootObject()) mMuEn_mediumPt_EndCap->Fill((*pfJets)[ijet].chargedMuEnergy());
	      mChMultiplicity_mediumPt_EndCap = dbe_->get(DirName+"/"+"ChMultiplicity_mediumPt_EndCap"); if(mChMultiplicity_mediumPt_EndCap && mChMultiplicity_mediumPt_EndCap->getRootObject())  mChMultiplicity_mediumPt_EndCap->Fill((*pfJets)[ijet].chargedMultiplicity());
	      mNeutMultiplicity_mediumPt_EndCap = dbe_->get(DirName+"/"+"NeutMultiplicity_mediumPt_EndCap"); if(mNeutMultiplicity_mediumPt_EndCap && mNeutMultiplicity_mediumPt_EndCap->getRootObject())  mNeutMultiplicity_mediumPt_EndCap->Fill((*pfJets)[ijet].neutralMultiplicity());
	      mMuMultiplicity_mediumPt_EndCap = dbe_->get(DirName+"/"+"MuMultiplicity_mediumPt_EndCap"); if(mMuMultiplicity_mediumPt_EndCap && mMuMultiplicity_mediumPt_EndCap->getRootObject())  mMuMultiplicity_mediumPt_EndCap->Fill((*pfJets)[ijet].muonMultiplicity());
	    }
	    if ((*pfJets)[ijet].pt()>140.) {
	      mCHFrac_highPt_EndCap = dbe_->get(DirName+"/"+"CHFrac_highPt_EndCap"); if (mCHFrac_highPt_EndCap &&  mCHFrac_highPt_EndCap->getRootObject()) mCHFrac_highPt_EndCap->Fill((*pfJets)[ijet].chargedHadronEnergyFraction());
	      mNHFrac_highPt_EndCap = dbe_->get(DirName+"/"+"NHFrac_highPt_EndCap"); if (mNHFrac_highPt_EndCap &&  mNHFrac_highPt_EndCap->getRootObject()) mNHFrac_highPt_EndCap->Fill((*pfJets)[ijet].neutralHadronEnergyFraction());
	      mPhFrac_highPt_EndCap = dbe_->get(DirName+"/"+"PhFrac_highPt_EndCap"); if (mPhFrac_highPt_EndCap &&  mPhFrac_highPt_EndCap->getRootObject()) mPhFrac_highPt_EndCap->Fill((*pfJets)[ijet].neutralEmEnergyFraction());
	      mElFrac_highPt_EndCap = dbe_->get(DirName+"/"+"ElFrac_highPt_EndCap"); if (mElFrac_highPt_EndCap &&  mElFrac_highPt_EndCap->getRootObject()) mElFrac_highPt_EndCap->Fill((*pfJets)[ijet].chargedEmEnergyFraction());
	      mMuFrac_highPt_EndCap = dbe_->get(DirName+"/"+"MuFrac_highPt_EndCap"); if (mMuFrac_highPt_EndCap &&  mMuFrac_highPt_EndCap->getRootObject()) mMuFrac_highPt_EndCap->Fill((*pfJets)[ijet].chargedMuEnergyFraction());
  	      mCHEn_highPt_EndCap = dbe_->get(DirName+"/"+"CHEn_highPt_EndCap"); if (mCHEn_highPt_EndCap &&  mCHEn_highPt_EndCap->getRootObject()) mCHEn_highPt_EndCap->Fill((*pfJets)[ijet].chargedHadronEnergy());
	      mNHEn_highPt_EndCap = dbe_->get(DirName+"/"+"NHEn_highPt_EndCap"); if (mNHEn_highPt_EndCap &&  mNHEn_highPt_EndCap->getRootObject()) mNHEn_highPt_EndCap->Fill((*pfJets)[ijet].neutralHadronEnergy());
	      mPhEn_highPt_EndCap = dbe_->get(DirName+"/"+"PhEn_highPt_EndCap"); if (mPhEn_highPt_EndCap &&  mPhEn_highPt_EndCap->getRootObject()) mPhEn_highPt_EndCap->Fill((*pfJets)[ijet].neutralEmEnergy());
	      mElEn_highPt_EndCap = dbe_->get(DirName+"/"+"ElEn_highPt_EndCap"); if (mElEn_highPt_EndCap &&  mElEn_highPt_EndCap->getRootObject()) mElEn_highPt_EndCap->Fill((*pfJets)[ijet].chargedEmEnergy());
	      mMuEn_highPt_EndCap = dbe_->get(DirName+"/"+"MuEn_highPt_EndCap"); if (mMuEn_highPt_EndCap &&  mMuEn_highPt_EndCap->getRootObject()) mMuEn_highPt_EndCap->Fill((*pfJets)[ijet].chargedMuEnergy());
	      mChMultiplicity_highPt_EndCap = dbe_->get(DirName+"/"+"ChMultiplicity_highPt_EndCap"); if(mChMultiplicity_highPt_EndCap && mChMultiplicity_highPt_EndCap->getRootObject())  mChMultiplicity_highPt_EndCap->Fill((*pfJets)[ijet].chargedMultiplicity());
	      mNeutMultiplicity_highPt_EndCap = dbe_->get(DirName+"/"+"NeutMultiplicity_highPt_EndCap"); if(mNeutMultiplicity_highPt_EndCap && mNeutMultiplicity_highPt_EndCap->getRootObject())  mNeutMultiplicity_highPt_EndCap->Fill((*pfJets)[ijet].neutralMultiplicity());
	      mMuMultiplicity_highPt_EndCap = dbe_->get(DirName+"/"+"MuMultiplicity_highPt_EndCap"); if(mMuMultiplicity_highPt_EndCap && mMuMultiplicity_highPt_EndCap->getRootObject())  mMuMultiplicity_highPt_EndCap->Fill((*pfJets)[ijet].muonMultiplicity());
	    }
	    mCHFracVSpT_EndCap = dbe_->get(DirName+"/"+"CHFracVSpT_EndCap"); if(mCHFracVSpT_EndCap && mCHFracVSpT_EndCap->getRootObject()) mCHFracVSpT_EndCap->Fill((*pfJets)[ijet].pt(),(*pfJets)[ijet].chargedHadronEnergyFraction());
	    mNHFracVSpT_EndCap = dbe_->get(DirName+"/"+"NHFracVSpT_EndCap");if (mNHFracVSpT_EndCap && mNHFracVSpT_EndCap->getRootObject()) mNHFracVSpT_EndCap->Fill((*pfJets)[ijet].pt(),(*pfJets)[ijet].neutralHadronEnergyFraction());
	    mPhFracVSpT_EndCap = dbe_->get(DirName+"/"+"PhFracVSpT_EndCap");if (mPhFracVSpT_EndCap && mPhFracVSpT_EndCap->getRootObject()) mPhFracVSpT_EndCap->Fill((*pfJets)[ijet].pt(),(*pfJets)[ijet].neutralEmEnergyFraction());
	    mElFracVSpT_EndCap = dbe_->get(DirName+"/"+"ElFracVSpT_EndCap");if (mElFracVSpT_EndCap && mElFracVSpT_EndCap->getRootObject()) mElFracVSpT_EndCap->Fill((*pfJets)[ijet].pt(),(*pfJets)[ijet].chargedEmEnergyFraction());
	    mMuFracVSpT_EndCap = dbe_->get(DirName+"/"+"MuFracVSpT_EndCap");if (mMuFracVSpT_EndCap && mMuFracVSpT_EndCap->getRootObject()) mMuFracVSpT_EndCap->Fill((*pfJets)[ijet].pt(),(*pfJets)[ijet].chargedMuEnergyFraction());
	  }else{
	    mHFrac_Forward = dbe_->get(DirName+"/"+"HFrac_Forward"); if (mHFrac_Forward && mHFrac_Forward->getRootObject())    mHFrac_Forward->Fill((*pfJets)[ijet].chargedHadronEnergyFraction() + (*pfJets)[ijet].neutralHadronEnergyFraction()+(*pfJets)[ijet].HFHadronEnergyFraction ());	
	    mEFrac_Forward = dbe_->get(DirName+"/"+"EFrac_Forward"); if (mEFrac_Forward && mEFrac_Forward->getRootObject()) mEFrac_Forward->Fill ((*pfJets)[ijet].chargedEmEnergyFraction() + (*pfJets)[ijet].neutralEmEnergyFraction()+(*pfJets)[ijet].HFEMEnergyFraction ());
	    //fractions
	    if ((*pfJets)[ijet].pt()<=50.) {
	      mHFEFrac_lowPt_Forward = dbe_->get(DirName+"/"+"HFEFrac_lowPt_Forward"); if(mHFEFrac_lowPt_Forward && mHFEFrac_lowPt_Forward->getRootObject()) mHFEFrac_lowPt_Forward->Fill((*pfJets)[ijet].HFEMEnergyFraction());
	      mHFEFrac_lowPt_Forward = dbe_->get(DirName+"/"+"HFEFrac_lowPt_Forward"); if(mHFHFrac_lowPt_Forward && mHFHFrac_lowPt_Forward->getRootObject()) mHFHFrac_lowPt_Forward->Fill((*pfJets)[ijet].HFHadronEnergyFraction());
	      mHFEEn_lowPt_Forward = dbe_->get(DirName+"/"+"HFEEn_lowPt_Forward");     if(mHFEEn_lowPt_Forward && mHFEEn_lowPt_Forward->getRootObject())     mHFEEn_lowPt_Forward->Fill((*pfJets)[ijet].HFEMEnergy());
	      mHFHEn_lowPt_Forward = dbe_->get(DirName+"/"+"HFHEn_lowPt_Forward");    if(mHFHEn_lowPt_Forward && mHFHEn_lowPt_Forward->getRootObject())     mHFHEn_lowPt_Forward->Fill((*pfJets)[ijet].HFHadronEnergy());
	      mChMultiplicity_lowPt_Barrel = dbe_->get(DirName+"/"+"ChMultiplicity_lowPt_Barrel"); if(mChMultiplicity_lowPt_Forward && mChMultiplicity_lowPt_Forward->getRootObject())  mChMultiplicity_lowPt_Forward->Fill((*pfJets)[ijet].chargedMultiplicity());
	      mNeutMultiplicity_lowPt_Barrel = dbe_->get(DirName+"/"+"NeutMultiplicity_lowPt_Barrel"); if(mNeutMultiplicity_lowPt_Forward && mNeutMultiplicity_lowPt_Forward->getRootObject())  mNeutMultiplicity_lowPt_Forward->Fill((*pfJets)[ijet].neutralMultiplicity());
	    }
	    if ((*pfJets)[ijet].pt()>50. && (*pfJets)[ijet].pt()<=140.) {
	      mHFEFrac_mediumPt_Forward = dbe_->get(DirName+"/"+"HFEFrac_mediumPt_Forward"); if(mHFEFrac_mediumPt_Forward && mHFEFrac_mediumPt_Forward->getRootObject()) mHFEFrac_mediumPt_Forward->Fill((*pfJets)[ijet].HFEMEnergyFraction());
	      mHFEFrac_mediumPt_Forward = dbe_->get(DirName+"/"+"HFEFrac_mediumPt_Forward"); if(mHFHFrac_mediumPt_Forward && mHFHFrac_mediumPt_Forward->getRootObject()) mHFHFrac_mediumPt_Forward->Fill((*pfJets)[ijet].HFHadronEnergyFraction());
	      mHFEEn_mediumPt_Forward = dbe_->get(DirName+"/"+"HFEEn_mediumPt_Forward");     if(mHFEEn_mediumPt_Forward && mHFEEn_mediumPt_Forward->getRootObject())     mHFEEn_mediumPt_Forward->Fill((*pfJets)[ijet].HFEMEnergy());
	      mHFHEn_mediumPt_Forward = dbe_->get(DirName+"/"+"HFHEn_mediumPt_Forward");    if(mHFHEn_mediumPt_Forward && mHFHEn_mediumPt_Forward->getRootObject())     mHFHEn_mediumPt_Forward->Fill((*pfJets)[ijet].HFHadronEnergy());
	      mChMultiplicity_mediumPt_Barrel = dbe_->get(DirName+"/"+"ChMultiplicity_mediumPt_Barrel"); if(mChMultiplicity_mediumPt_Forward && mChMultiplicity_mediumPt_Forward->getRootObject())  mChMultiplicity_mediumPt_Forward->Fill((*pfJets)[ijet].chargedMultiplicity());
	      mNeutMultiplicity_mediumPt_Barrel = dbe_->get(DirName+"/"+"NeutMultiplicity_mediumPt_Barrel"); if(mNeutMultiplicity_mediumPt_Forward && mNeutMultiplicity_mediumPt_Forward->getRootObject())  mNeutMultiplicity_mediumPt_Forward->Fill((*pfJets)[ijet].neutralMultiplicity());
	    }
	    if ((*pfJets)[ijet].pt()>140.) {
	      mHFEFrac_highPt_Forward = dbe_->get(DirName+"/"+"HFEFrac_highPt_Forward"); if(mHFEFrac_highPt_Forward && mHFEFrac_highPt_Forward->getRootObject()) mHFEFrac_highPt_Forward->Fill((*pfJets)[ijet].HFEMEnergyFraction());
	      mHFEFrac_highPt_Forward = dbe_->get(DirName+"/"+"HFEFrac_highPt_Forward"); if(mHFHFrac_highPt_Forward && mHFHFrac_highPt_Forward->getRootObject()) mHFHFrac_highPt_Forward->Fill((*pfJets)[ijet].HFHadronEnergyFraction());
	      mHFEEn_highPt_Forward = dbe_->get(DirName+"/"+"HFEEn_highPt_Forward");     if(mHFEEn_highPt_Forward && mHFEEn_highPt_Forward->getRootObject())     mHFEEn_highPt_Forward->Fill((*pfJets)[ijet].HFEMEnergy());
	      mHFHEn_highPt_Forward = dbe_->get(DirName+"/"+"HFHEn_highPt_Forward");    if(mHFHEn_highPt_Forward && mHFHEn_highPt_Forward->getRootObject())     mHFHEn_highPt_Forward->Fill((*pfJets)[ijet].HFHadronEnergy());
	      mChMultiplicity_highPt_Barrel = dbe_->get(DirName+"/"+"ChMultiplicity_highPt_Barrel"); if(mChMultiplicity_highPt_Forward && mChMultiplicity_highPt_Forward->getRootObject())  mChMultiplicity_highPt_Forward->Fill((*pfJets)[ijet].chargedMultiplicity());
	      mNeutMultiplicity_highPt_Barrel = dbe_->get(DirName+"/"+"NeutMultiplicity_highPt_Barrel"); if(mNeutMultiplicity_highPt_Forward && mNeutMultiplicity_highPt_Forward->getRootObject())  mNeutMultiplicity_highPt_Forward->Fill((*pfJets)[ijet].neutralMultiplicity());
	    }
	  }
	  mChargedHadronEnergy = dbe_->get(DirName+"/"+"mChargedHadronEnergy"); if (mChargedHadronEnergy && mChargedHadronEnergy->getRootObject())  mChargedHadronEnergy->Fill ((*pfJets)[ijet].chargedHadronEnergy());
	  mNeutralHadronEnergy = dbe_->get(DirName+"/"+"mNeutralHadronEnergy"); if (mNeutralHadronEnergy && mNeutralHadronEnergy->getRootObject())  mNeutralHadronEnergy->Fill ((*pfJets)[ijet].neutralHadronEnergy());
	  mChargedEmEnergy = dbe_->get(DirName+"/"+"mChargedEmEnergy"); if (mChargedEmEnergy && mChargedEmEnergy->getRootObject()) mChargedEmEnergy->Fill((*pfJets)[ijet].chargedEmEnergy());
	  mChargedMuEnergy = dbe_->get(DirName+"/"+"mChargedMuEnergy"); if (mChargedMuEnergy && mChargedMuEnergy->getRootObject()) mChargedMuEnergy->Fill ((*pfJets)[ijet].chargedMuEnergy ());
	  mNeutralEmEnergy = dbe_->get(DirName+"/"+"mNeutralEmEnergy"); if (mNeutralEmEnergy && mNeutralEmEnergy->getRootObject()) mNeutralEmEnergy->Fill((*pfJets)[ijet].neutralEmEnergy());
	  mChargedMultiplicity = dbe_->get(DirName+"/"+"mChargedMultiplicity"); if (mChargedMultiplicity && mChargedMultiplicity->getRootObject()) mChargedMultiplicity->Fill((*pfJets)[ijet].chargedMultiplicity());
	  mNeutralMultiplicity = dbe_->get(DirName+"/"+"mNeutralMultiplicity"); if (mNeutralMultiplicity && mNeutralMultiplicity->getRootObject()) mNeutralMultiplicity->Fill((*pfJets)[ijet].neutralMultiplicity());
	  mMuonMultiplicity = dbe_->get(DirName+"/"+"mMuonMultiplicity"); if (mMuonMultiplicity && mMuonMultiplicity->getRootObject()) mMuonMultiplicity->Fill ((*pfJets)[ijet]. muonMultiplicity());
	  //_______________________________________________________
	  mNeutralFraction = dbe_->get(DirName+"/"+"mNeutralFraction");if (mNeutralFraction && mNeutralFraction->getRootObject()) mNeutralFraction->Fill ((double)(*pfJets)[ijet].neutralMultiplicity()/(double)(*pfJets)[ijet].nConstituents());

	  mHFrac_profile = dbe_->get(DirName+"/"+"HFrac_profile"); if (mHFrac_profile && mHFrac_profile->getRootObject()) mHFrac_profile       ->Fill(numPV, (*pfJets)[ijet].chargedHadronEnergyFraction() + (*pfJets)[ijet].neutralHadronEnergyFraction());
	  mEFrac_profile = dbe_->get(DirName+"/"+"EFrac_profile"); if (mEFrac_profile && mEFrac_profile->getRootObject()) mEFrac_profile       ->Fill(numPV, (*pfJets)[ijet].chargedEmEnergyFraction()     + (*pfJets)[ijet].neutralEmEnergyFraction());
	  
	  mChargedHadronEnergy_profile = dbe_->get(DirName+"/"+"mChargedHadronEnergy_profile"); if (mChargedHadronEnergy_profile && mChargedHadronEnergy_profile->getRootObject()) mChargedHadronEnergy_profile->Fill(numPV, (*pfJets)[ijet].chargedHadronEnergy());
	  mNeutralHadronEnergy_profile = dbe_->get(DirName+"/"+"mNeutralHadronEnergy_profile");if (mNeutralHadronEnergy_profile && mNeutralHadronEnergy_profile->getRootObject()) mNeutralHadronEnergy_profile->Fill(numPV, (*pfJets)[ijet].neutralHadronEnergy());
	  mChargedEmEnergy_profile = dbe_->get(DirName+"/"+"mChargedEmEnergy_profile"); if (mChargedEmEnergy_profile && mChargedEmEnergy_profile->getRootObject())     mChargedEmEnergy_profile    ->Fill(numPV, (*pfJets)[ijet].chargedEmEnergy());
	  mChargedMuEnergy_profile = dbe_->get(DirName+"/"+"mChargedMuEnergy_profile");if (mChargedMuEnergy_profile && mChargedMuEnergy_profile->getRootObject())     mChargedMuEnergy_profile    ->Fill(numPV, (*pfJets)[ijet].chargedMuEnergy ());
	  mNeutralEmEnergy_profile = dbe_->get(DirName+"/"+"mNeutralEmEnergy_profile");if (mNeutralEmEnergy_profile && mNeutralEmEnergy_profile->getRootObject())     mNeutralEmEnergy_profile    ->Fill(numPV, (*pfJets)[ijet].neutralEmEnergy());
	  mChargedMultiplicity_profile = dbe_->get(DirName+"/"+"mChargedMultiplicity_profile"); if (mChargedMultiplicity_profile && mChargedMultiplicity_profile->getRootObject()) mChargedMultiplicity_profile->Fill(numPV, (*pfJets)[ijet].chargedMultiplicity());
	  mNeutralMultiplicity_profile = dbe_->get(DirName+"/"+"mNeutralMultiplicity_profile");if (mNeutralMultiplicity_profile && mNeutralMultiplicity_profile->getRootObject()) mNeutralMultiplicity_profile->Fill(numPV, (*pfJets)[ijet].neutralMultiplicity());
	  mMuonMultiplicity_profile = dbe_->get(DirName+"/"+"mMuonMultiplicity_profile"); if (mMuonMultiplicity_profile && mMuonMultiplicity_profile->getRootObject())    mMuonMultiplicity_profile   ->Fill(numPV, (*pfJets)[ijet].muonMultiplicity());

	}//cleaned PFJets
      }//PFJet specific loop
      //IDs have been defined by now

      //if only uncorrected jets but no corrected jets over threshold pass on
      if(!pass_corrected){
	continue;
      }      
      if(correctedJet.pt()>pt1){
	pt3=pt2;
	ind3=ind2;
	cleaned_third_jet=cleaned_second_jet;
	pt2=pt1;
	ind2=ind1;
	cleaned_second_jet=cleaned_first_jet;
	pt1=correctedJet.pt();
	ind1=ijet;
	cleaned_first_jet=jetpassid;
      } else if(correctedJet.pt()>pt2){
	pt3=pt2;
	ind3=ind2;
	cleaned_third_jet=cleaned_second_jet;
	pt2=correctedJet.pt();
	ind2=ijet;
	cleaned_second_jet=jetpassid;
      } else if(correctedJet.pt()>pt3){
	pt3=correctedJet.pt();
	ind3=ijet;
	cleaned_third_jet=jetpassid;
      }
      if(cleaned_third_jet){
      }
      //after jettype specific variables are filled -> perform histograms for all jets
      //fill JetID efficiencies if uncleaned selection is chosen
      if(!jetCleaningFlag_){
	if(jetpassid) {
	  mLooseJIDPassFractionVSeta = dbe_->get(DirName+"/"+"JetIDPassFractionVSeta"); if (mLooseJIDPassFractionVSeta && mLooseJIDPassFractionVSeta->getRootObject())  mLooseJIDPassFractionVSeta->Fill(correctedJet.eta(),1.);
	  mLooseJIDPassFractionVSpt = dbe_->get(DirName+"/"+"JetIDPassFractionVSpt"); if (mLooseJIDPassFractionVSpt && mLooseJIDPassFractionVSpt->getRootObject()) mLooseJIDPassFractionVSpt->Fill(correctedJet.pt(),1.);
	  if(correctedJet.eta()<3.0){
	    mLooseJIDPassFractionVSptNoHF= dbe_->get(DirName+"/"+"JetIDPassFractionVSptNoHF"); if (mLooseJIDPassFractionVSptNoHF && mLooseJIDPassFractionVSptNoHF->getRootObject()) mLooseJIDPassFractionVSptNoHF->Fill(correctedJet.pt(),1.);
	  }
	} else {
	  mLooseJIDPassFractionVSeta = dbe_->get(DirName+"/"+"JetIDPassFractionVSeta"); if (mLooseJIDPassFractionVSeta && mLooseJIDPassFractionVSeta->getRootObject()) mLooseJIDPassFractionVSeta->Fill(correctedJet.eta(),0.);
	  mLooseJIDPassFractionVSpt = dbe_->get(DirName+"/"+"JetIDPassFractionVSpt"); if (mLooseJIDPassFractionVSpt && mLooseJIDPassFractionVSpt->getRootObject()) mLooseJIDPassFractionVSpt->Fill(correctedJet.pt(),0.);
	  if(correctedJet.eta()<3.0){
	    mLooseJIDPassFractionVSptNoHF= dbe_->get(DirName+"/"+"JetIDPassFractionVSptNoHF"); if (mLooseJIDPassFractionVSptNoHF && mLooseJIDPassFractionVSptNoHF->getRootObject()) mLooseJIDPassFractionVSptNoHF->Fill(correctedJet.pt(),0.);
	  }
	}
      }
      //here we so far consider calojets ->check for PFJets and JPT jets again
      if(Thiscleaned && pass_corrected){//might be softer than loose jet ID 
	numofjets++;
	
	if(isCaloJet_){
	  jetME = dbe_->get(DirName+"/"+"jetReco"); if(jetME && jetME->getRootObject()) jetME->Fill(1);
	  mJetEnergyCorr = dbe_->get(DirName+"/"+"JetEnergyCorr"); if(mJetEnergyCorr && mJetEnergyCorr->getRootObject()) mJetEnergyCorr->Fill(correctedJet.pt()/(*caloJets)[ijet].pt());
	  mJetEnergyCorrVSEta = dbe_->get(DirName+"/"+"JetEnergyCorrVSEta"); if(mJetEnergyCorrVSEta && mJetEnergyCorrVSEta->getRootObject()) mJetEnergyCorrVSEta->Fill(correctedJet.eta(),correctedJet.pt()/(*caloJets)[ijet].pt());
	  mJetEnergyCorrVSPt = dbe_->get(DirName+"/"+"JetEnergyCorrVSPt"); if(mJetEnergyCorrVSPt && mJetEnergyCorrVSPt->getRootObject()) mJetEnergyCorrVSPt->Fill(correctedJet.pt(),correctedJet.pt()/(*caloJets)[ijet].pt());
	}
	if(isPFJet_){
	  jetME = dbe_->get(DirName+"/"+"jetReco"); if(jetME && jetME->getRootObject()) jetME->Fill(2);
	  mJetEnergyCorr = dbe_->get(DirName+"/"+"JetEnergyCorr"); if(mJetEnergyCorr && mJetEnergyCorr->getRootObject()) mJetEnergyCorr->Fill(correctedJet.pt()/(*pfJets)[ijet].pt());
	  mJetEnergyCorrVSEta = dbe_->get(DirName+"/"+"JetEnergyCorrVSEta"); if(mJetEnergyCorrVSEta && mJetEnergyCorrVSEta->getRootObject())mJetEnergyCorrVSEta->Fill(correctedJet.eta(),correctedJet.pt()/(*pfJets)[ijet].pt());
	  mJetEnergyCorrVSPt = dbe_->get(DirName+"/"+"JetEnergyCorrVSPt"); if(mJetEnergyCorrVSPt && mJetEnergyCorrVSPt->getRootObject()) mJetEnergyCorrVSPt->Fill(correctedJet.pt(),correctedJet.pt()/(*pfJets)[ijet].pt());
	}
	if(isJPTJet_){
	  jetME = dbe_->get(DirName+"/"+"jetReco"); if(jetME && jetME->getRootObject()) jetME->Fill(3);
	  mJetEnergyCorr = dbe_->get(DirName+"/"+"JetEnergyCorr"); if(mJetEnergyCorr && mJetEnergyCorr->getRootObject()) mJetEnergyCorr->Fill(correctedJet.pt()/(*jptJets)[ijet].pt());
	  mJetEnergyCorrVSEta = dbe_->get(DirName+"/"+"JetEnergyCorrVSEta"); if(mJetEnergyCorrVSEta && mJetEnergyCorrVSEta->getRootObject())mJetEnergyCorrVSEta->Fill(correctedJet.eta(),correctedJet.pt()/(*jptJets)[ijet].pt());
	  mJetEnergyCorrVSPt = dbe_->get(DirName+"/"+"JetEnergyCorrVSPt"); if(mJetEnergyCorrVSPt && mJetEnergyCorrVSPt->getRootObject()) mJetEnergyCorrVSPt->Fill(correctedJet.pt(),correctedJet.pt()/(*jptJets)[ijet].pt());
	}
	// --- Event passed the low pt jet trigger
	if (jetLoPass_ == 1) {	  
	  mPhi_Lo = dbe_->get(DirName+"/"+"Phi_Lo"); if (mPhi_Lo && mPhi_Lo->getRootObject()) mPhi_Lo->Fill (correctedJet.phi());
	  mPt_Lo = dbe_->get(DirName+"/"+"Pt_Lo"); if (mPt_Lo && mPt_Lo->getRootObject())  mPt_Lo->Fill (correctedJet.pt());
	  
	}
	// --- Event passed the high pt jet trigger
	if (jetHiPass_ == 1&& correctedJet.pt()>100. ) {
	  mEta_Hi = dbe_->get(DirName+"/"+"Eta_Hi"); if (mEta_Hi && mEta_Hi->getRootObject()) mEta_Hi->Fill (correctedJet.eta());
	  mPhi_Hi = dbe_->get(DirName+"/"+"Phi_Hi"); if (mPhi_Hi && mPhi_Hi->getRootObject()) mPhi_Hi->Fill (correctedJet.phi());
	  mPt_Hi = dbe_->get(DirName+"/"+"Pt_Hi"); if (mPt_Hi && mPt_Hi->getRootObject()) mPt_Hi->Fill (correctedJet.pt());		     		    
	}
	mPt = dbe_->get(DirName+"/"+"Pt"); if (mPt && mPt->getRootObject()) mPt->Fill (correctedJet.pt());
	mPt_1 = dbe_->get(DirName+"/"+"Pt_1"); if (mPt_1 && mPt_1->getRootObject())  mPt_1->Fill (correctedJet.pt());
	mPt_2 = dbe_->get(DirName+"/"+"Pt_2"); if (mPt_2 && mPt_2->getRootObject()) mPt_2->Fill (correctedJet.pt());
	mPt_3 = dbe_->get(DirName+"/"+"Pt_3"); if (mPt_3 && mPt_3->getRootObject()) mPt_3->Fill (correctedJet.pt());
	mEta = dbe_->get(DirName+"/"+"Eta"); if (mEta && mEta->getRootObject()) mEta->Fill (correctedJet.eta());
	mPhi = dbe_->get(DirName+"/"+"Phi"); if (mPhi && mPhi->getRootObject())  mPhi->Fill (correctedJet.phi());	 
	
	mPhiVSEta = dbe_->get(DirName+"/"+"PhiVSEta"); if (mPhiVSEta && mPhiVSEta->getRootObject()) mPhiVSEta->Fill(correctedJet.eta(),correctedJet.phi());
	if(!isJPTJet_){
	  mConstituents = dbe_->get(DirName+"/"+"Constituents"); if (mConstituents && mConstituents->getRootObject()) mConstituents->Fill (correctedJet.nConstituents());
	}
	// Fill NPV profiles
	//--------------------------------------------------------------------
	mPt_profile = dbe_->get(DirName+"/"+"Pt_profile"); if (mPt_profile && mPt_profile->getRootObject())  mPt_profile ->Fill(numPV, correctedJet.pt());
	mEta_profile = dbe_->get(DirName+"/"+"Eta_profile"); if (mEta_profile && mEta_profile->getRootObject())  mEta_profile         ->Fill(numPV, correctedJet.eta());
	mPhi_profile = dbe_->get(DirName+"/"+"Phi_profile"); if (mPhi_profile && mPhi_profile->getRootObject())  mPhi_profile         ->Fill(numPV, correctedJet.phi());
	if(!isJPTJet_){
	  mConstituents_profile = dbe_->get(DirName+"/"+"Constituents_profile"); if (mConstituents_profile && mConstituents_profile->getRootObject())  mConstituents_profile->Fill(numPV, correctedJet.nConstituents());
	}
	if (fabs(correctedJet.eta()) <= 1.3) {
	  mPt_Barrel = dbe_->get(DirName+"/"+"Pt_Barrel"); if (mPt_Barrel && mPt_Barrel->getRootObject()) mPt_Barrel->Fill (correctedJet.pt());
	  mPhi_Barrel = dbe_->get(DirName+"/"+"Phi_Barrel"); if (mPhi_Barrel && mPhi_Barrel->getRootObject()) mPhi_Barrel->Fill (correctedJet.phi());
	  //if (mE_Barrel)    mE_Barrel->Fill (correctedJet.energy());
	  if(!isJPTJet_){
	   mConstituents_Barrel = dbe_->get(DirName+"/"+"Constituents_Barrel"); if (mConstituents_Barrel && mConstituents_Barrel->getRootObject()) mConstituents_Barrel->Fill(correctedJet.nConstituents());
	  }
	}else if (fabs(correctedJet.eta()) <= 3) {
	  mPt_EndCap = dbe_->get(DirName+"/"+"Pt_EndCap"); if (mPt_EndCap && mPt_EndCap->getRootObject()) mPt_EndCap->Fill (correctedJet.pt());
	  mPhi_EndCap = dbe_->get(DirName+"/"+"Phi_EndCap"); if (mPhi_EndCap && mPhi_EndCap->getRootObject())  mPhi_EndCap->Fill (correctedJet.phi());
	  //if (mE_EndCap)    mE_EndCap->Fill (correctedJet.energy());
	  if(!isJPTJet_){
	    mConstituents_EndCap = dbe_->get(DirName+"/"+"Constituents_EndCap"); if (mConstituents_EndCap && mConstituents_EndCap->getRootObject())  mConstituents_EndCap->Fill(correctedJet.nConstituents());
	  }
	}else{
	  mPt_Forward = dbe_->get(DirName+"/"+"Pt_Forward"); if (mPt_Forward && mPt_Forward->getRootObject()) mPt_Forward->Fill (correctedJet.pt());
	  mPhi_Forward = dbe_->get(DirName+"/"+"Phi_Forward"); if (mPhi_Forward && mPhi_Forward->getRootObject()) mPhi_Forward->Fill (correctedJet.phi());
	  //if (mE_Forward)    mE_Forward->Fill (correctedJet.energy());
	  if(!isJPTJet_){
	    mConstituents_Forward = dbe_->get(DirName+"/"+"Constituents_Forward"); if (mConstituents_Forward && mConstituents_Forward->getRootObject())   mConstituents_Forward->Fill(correctedJet.nConstituents());
	  }
	}
      }
    }// the selection of all jets --> inclusive selection
  }//loop over uncorrected jets 
  
  if(!diJetSelectionFlag_ ){
    mNJets = dbe_->get(DirName+"/"+"NJets"); if (mNJets && mNJets->getRootObject())  mNJets->Fill (numofjets);
    mNJets_profile = dbe_->get(DirName+"/"+"NJets_profile"); if (mNJets_profile && mNJets_profile->getRootObject())  mNJets_profile->Fill(numPV, numofjets);
  }
  sort(recoJets.begin(),recoJets.end(),jetSortingRule);
  
  //for non dijet selection, otherwise numofjets==0
  if(numofjets>0){
    //check ID of the leading jet
    bool Thiscleaned=true;
    if(isCaloJet_){
      reco::CaloJetRef calojetref1(caloJets, ind1);
      reco::JetID jetID1 = (*jetID_ValueMap_Handle)[calojetref1];
      if(jetCleaningFlag_){
	Thiscleaned = jetIDFunctor((*caloJets)[ind1], jetID1);
      }
    }
    if(isJPTJet_){
      const edm::RefToBase<reco::Jet>&  rawJet1 = (*jptJets)[ind1].getCaloJetRef();
      //change that step here
      //check jet is correctable by JPT
      //if ( fabs(rawJet->eta()) > 2.1) return;
      
      try {
	const reco::CaloJet *rawCaloJet1 = dynamic_cast<const reco::CaloJet*>(&*rawJet1);	
	reco::CaloJetRef const theCaloJetRef1 = (rawJet1).castTo<reco::CaloJetRef>();
	reco::JetID jetID1 = (*jetID_ValueMap_Handle)[theCaloJetRef1];
	if(jetCleaningFlag_){
	  Thiscleaned = jetIDFunctor(*rawCaloJet1, jetID1);
	}
      } catch (const std::bad_cast&) {
	edm::LogError("JetPlusTrackDQM") << "Failed to cast raw jet to CaloJet. JPT Jet does not appear to have been built from a CaloJet. "
					 << "Histograms not filled. ";
	return;
      }
    }
    if(isPFJet_){
      if(jetCleaningFlag_){
	Thiscleaned = pfjetIDFunctor((*pfJets)[ind1]);
      }
    }
    if(Thiscleaned){
      mEtaFirst = dbe_->get(DirName+"/"+"EtaFirst"); if (mEtaFirst && mEtaFirst->getRootObject())  mEtaFirst->Fill ((recoJets)[0].eta());
      mPhiFirst = dbe_->get(DirName+"/"+"PhiFirst"); if (mPhiFirst && mPhiFirst->getRootObject())  mPhiFirst->Fill ((recoJets)[0].phi());
      mPtFirst = dbe_->get(DirName+"/"+"PtFirst"); if (mPtFirst && mPtFirst->getRootObject())  mPtFirst->Fill ((recoJets)[0].pt());
      if(numofjets>1) {
	//first jet fine -> now check second jet
	Thiscleaned=true;
	if(isCaloJet_){
	  reco::CaloJetRef calojetref2(caloJets, ind2);
	  reco::JetID jetID2 = (*jetID_ValueMap_Handle)[calojetref2];
	  if(jetCleaningFlag_){
	    Thiscleaned = jetIDFunctor((*caloJets)[ind2], jetID2);
	  }
	}
	if(isJPTJet_){
	  const edm::RefToBase<reco::Jet>&  rawJet2 = (*jptJets)[ind2].getCaloJetRef();
	  //change that step here
	  //check jet is correctable by JPT
	  //if ( fabs(rawJet->eta()) > 2.1) return;  
	  try {
	    const reco::CaloJet *rawCaloJet2 = dynamic_cast<const reco::CaloJet*>(&*rawJet2);
	    reco::CaloJetRef const theCaloJetRef2 = (rawJet2).castTo<reco::CaloJetRef>();
	    reco::JetID jetID2 = (*jetID_ValueMap_Handle)[theCaloJetRef2];
	    if(jetCleaningFlag_){
	      Thiscleaned = jetIDFunctor(*rawCaloJet2, jetID2);
	    }
	  } catch (const std::bad_cast&) {
	    edm::LogError("JetPlusTrackDQM") << "Failed to cast raw jet to CaloJet. JPT Jet does not appear to have been built from a CaloJet. "
					     << "Histograms not filled. ";
	    return;
	  }
	  if(Thiscleaned){
	    mPtSecond = dbe_->get(DirName+"/"+"PtSecond"); if (mPtSecond && mPtSecond->getRootObject())  mPtSecond->Fill(recoJets[1].pt());
	  }
	  if(numofjets>2){
	    Thiscleaned=true;
	    const edm::RefToBase<reco::Jet>&  rawJet3 = (*jptJets)[ind3].getCaloJetRef();
	    //change that step here
	    //check jet is correctable by JPT
	    //if ( fabs(rawJet->eta()) > 2.1) return;
	    
	    try {
	      const reco::CaloJet *rawCaloJet3 = dynamic_cast<const reco::CaloJet*>(&*rawJet3);
	      reco::CaloJetRef const theCaloJetRef3 = (rawJet3).castTo<reco::CaloJetRef>();
	      reco::JetID jetID3 = (*jetID_ValueMap_Handle)[theCaloJetRef3];
	      if(jetCleaningFlag_){
		Thiscleaned = jetIDFunctor(*rawCaloJet3, jetID3);
	      }
	    } catch (const std::bad_cast&) {
	      edm::LogError("JetPlusTrackDQM") << "Failed to cast raw jet to CaloJet. JPT Jet does not appear to have been built from a CaloJet. "
					       << "Histograms not filled. ";
	      return;
	    }
	    if(Thiscleaned){
	     mPtThird = dbe_->get(DirName+"/"+"PtThird"); if (mPtThird && mPtThird->getRootObject())  mPtThird->Fill(recoJets[2].pt());
	    }
	  }
	}
	if(isPFJet_){
	  if(jetCleaningFlag_){
	    Thiscleaned = pfjetIDFunctor((*pfJets)[ind2]);
	  }
	}
	if(Thiscleaned){
	  double dphi=fabs((recoJets)[0].phi()-(recoJets)[1].phi());
	  if(dphi>acos(-1.)){
	    dphi=2*acos(-1.)-dphi;
	  }
	  mDPhi = dbe_->get(DirName+"/"+"DPhi"); if (mDPhi && mDPhi->getRootObject())  mDPhi->Fill (dphi);
	  //try here now the three jet dijet asymmetry stuff
	  if (fabs(recoJets[0].eta() < 1.4)) {
	    double pt_dijet = (recoJets[0].pt() + recoJets[1].pt())/2;      
	    double dPhiDJ = fabs(recoJets[0].phi()-recoJets[1].phi());
	    if(dPhiDJ>acos(-1.)){
	      dPhiDJ=2*acos(-1.)-dPhiDJ;
	    }
	    if (dPhiDJ > 2.7) {
	      double pt_probe;
	      double pt_barrel;
	      int jet1, jet2;
	      int randJet = rand() % 2;
	      if (fabs(recoJets[1].eta() < 1.4)) {
		if (randJet) {
		  jet1 = 0;
		  jet2 = 1;
		}
		else {
		  jet1 = 1;
		  jet2 = 0;
		}	  
		// ***Di-Jet Asymmetry****
		// * leading jets eta < 1.4
		// * leading jets dphi > 2.7
		// * pt_third jet < threshold
		// * A = (pt_1 - pt_2)/(pt_1 + pt_2)
		// * jets 1 and two are randomly ordered
		// **
		bool thirdJetCut = true;
		//that doesn't make any sense -> imagine you have 5 jets,
		//jet 3 is quite hard (almost as hard as the second jet, i.e. 200/80/79/20/15, cutoff is 30
		//the 4th and 5th jet are soft enough -> then you would fill the asymmetry twice, 
		//although jet 2 and 3 are basically identical
		//do third jet relative to second jet
		//JME-10-014 suggests pt3/pt_dijet<0.15
		if(ind3>0){
		  if (pt3 > asymmetryThirdJetCut_){
		    thirdJetCut=false;
		  } 
		}
		if(thirdJetCut){
		  double dijetAsymmetry =(recoJets[jet1].pt() - recoJets[jet2].pt()) / (recoJets[jet1].pt() + recoJets[jet2].pt());
		  mDijetAsymmetry = dbe_->get(DirName+"/"+"DijetAsymmetry"); if (mDijetAsymmetry && mDijetAsymmetry->getRootObject()) mDijetAsymmetry->Fill(dijetAsymmetry);
		}// end restriction on third jet pt in asymmetry calculation
		
	      }
	      else {
		jet1 = 0;
		jet2 = 1;
	      }
	      
	      pt_barrel = recoJets[jet1].pt();
	      pt_probe  = recoJets[jet2].pt();
	      
	      //dijet balance cuts
	      // ***Di-Jet Balance****
	      // * pt_dijet = (pt_probe+pt_barrel)/2
	      // * leading jets dphi > 2.7
	      // * reject evnets where pt_third/pt_dijet > 0.2
	      // * pv selection
	      // * B = (pt_probe - pt_barrel)/pt_dijet
	      // * select probe randomly from 2 jets if both leading jets are in the barrel
	      bool thirdJetCut = true;
	      if(ind3>0){
		if (pt3/pt_dijet > balanceThirdJetCut_){ 
		  thirdJetCut = false;
		}
	      }
	      if (thirdJetCut) {
		double dijetBalance = (pt_probe - pt_barrel) / pt_dijet;
		mDijetBalance = dbe_->get(DirName+"/"+"DijetBalance"); if (mDijetBalance && mDijetBalance->getRootObject()) mDijetBalance->Fill(dijetBalance);
	      }// end restriction on third jet pt ratio in balance calculation
	    }// dPhi > 2.7
	  }// leading jet eta cut for asymmetry and balance calculations
	  //end here test try for 3 jet asymmetry blabla
	  }	
	}
    }
  }//leading jet histograms for non dijet selection
  

  //dijet selection -> recoJets selection ensures jet hard enough and corrected (if possible)
  if(recoJets.size()>1 && diJetSelectionFlag_ ){
    //pt threshold checked before filling
    if(fabs(recoJets[0].eta())<3. && fabs(recoJets[1].eta())<3. ){
      //calculate dphi
      double dphi=fabs((recoJets)[0].phi()-(recoJets)[1].phi());
      if(dphi>acos(-1.)){
	dphi=2*acos(-1.)-dphi;
      } 
      if (mDPhi) mDPhi->Fill (dphi);
      //dphi cut
      if(fabs(dphi)>2.1){
	bool LoosecleanedFirstJet = false;
	bool LoosecleanedSecondJet = false;
	if(isCaloJet_){
	  reco::CaloJetRef calojetref1(caloJets, ind1);
	  reco::JetID jetID1 = (*jetID_ValueMap_Handle)[calojetref1];
	  LoosecleanedFirstJet = jetIDFunctor((*caloJets)[ind1], jetID1);
	  reco::CaloJetRef calojetref2(caloJets, ind2);
	  reco::JetID jetID2 = (*jetID_ValueMap_Handle)[calojetref2];
	  LoosecleanedSecondJet = jetIDFunctor((*caloJets)[ind2], jetID2);	
	  if (mN90Hits)         mN90Hits->Fill (jetID1.n90Hits);
	  if (mfHPD)            mfHPD->Fill (jetID1.fHPD);
	  if (mresEMF)         mresEMF->Fill (jetID1.restrictedEMF);
	  if (mfRBX)            mfRBX->Fill (jetID1.fRBX);	  
	  if (mN90Hits)         mN90Hits->Fill (jetID2.n90Hits);
	  if (mfHPD)            mfHPD->Fill (jetID2.fHPD);
	  if (mresEMF)         mresEMF->Fill (jetID2.restrictedEMF);
	  if (mfRBX)            mfRBX->Fill (jetID2.fRBX);
	}
	if(isJPTJet_){
	  const edm::RefToBase<reco::Jet>&  rawJet1 = (*jptJets)[ind1].getCaloJetRef();	
	  try {
	    const reco::CaloJet *rawCaloJet1 = dynamic_cast<const reco::CaloJet*>(&*rawJet1);
	    reco::CaloJetRef const theCaloJetRef1 = (rawJet1).castTo<reco::CaloJetRef>();
	    reco::JetID jetID1 = (*jetID_ValueMap_Handle)[theCaloJetRef1];
	    LoosecleanedFirstJet = jetIDFunctor(*rawCaloJet1, jetID1);
	  } catch (const std::bad_cast&) {
	    edm::LogError("JetPlusTrackDQM") << "Failed to cast raw jet to CaloJet lead jet in dijet selection. JPT Jet does not appear to have been built from a CaloJet. "
					     << "Histograms not filled. ";
	    return;
	  }
	  const edm::RefToBase<reco::Jet>&  rawJet2 = (*jptJets)[ind2].getCaloJetRef();	
	  try {
	    const reco::CaloJet *rawCaloJet2 = dynamic_cast<const reco::CaloJet*>(&*rawJet2);
	    reco::CaloJetRef const theCaloJetRef2 = (rawJet2).castTo<reco::CaloJetRef>();
	    reco::JetID jetID2 = (*jetID_ValueMap_Handle)[theCaloJetRef2];
	    LoosecleanedSecondJet = jetIDFunctor(*rawCaloJet2, jetID2);    
	  } catch (const std::bad_cast&) {
	    edm::LogError("JetPlusTrackDQM") << "Failed to cast raw jet to CaloJet lead jet in dijet selection. JPT Jet does not appear to have been built from a CaloJet. "
					     << "Histograms not filled. ";
	    return;
	  }
	}
	if(isPFJet_){
	  LoosecleanedFirstJet = pfjetIDFunctor((*pfJets)[ind1]);	
	  LoosecleanedSecondJet = pfjetIDFunctor((*pfJets)[ind2]);
	}	  
	if(LoosecleanedFirstJet && LoosecleanedSecondJet) { //only if both jets are (loose) cleaned
	  //fill histos for first jet
	  if (mPt)   mPt->Fill (recoJets[0].pt());
	  if (mEta)  mEta->Fill (recoJets[0].eta());
	  if (mPhi)  mPhi->Fill (recoJets[0].phi());
	  if (mPhiVSEta) mPhiVSEta->Fill(recoJets[0].eta(),recoJets[0].phi());
	  if(!isJPTJet_){
	    if (mConstituents) mConstituents->Fill (recoJets[0].nConstituents());
	  }
	  if (mPt)   mPt->Fill (recoJets[1].pt());
	  if (mEta)  mEta->Fill (recoJets[1].eta());
	  if (mPhi)  mPhi->Fill (recoJets[1].phi());
	  if (mPhiVSEta) mPhiVSEta->Fill(recoJets[1].eta(),recoJets[1].phi());
	  if(!isJPTJet_){
	    if (mConstituents) mConstituents->Fill (recoJets[1].nConstituents());
	  }
	  //PV profiles 
	  if (mPt_profile)           mPt_profile          ->Fill(numPV, recoJets[0].pt());
	  if (mEta_profile)          mEta_profile         ->Fill(numPV, recoJets[0].eta());
	  if (mPhi_profile)          mPhi_profile         ->Fill(numPV, recoJets[0].phi());
	  if(!isJPTJet_){
	    if (mConstituents_profile) mConstituents_profile->Fill(numPV, recoJets[0].nConstituents());
	  }
	  if (mPt_profile)           mPt_profile          ->Fill(numPV, recoJets[1].pt());
	  if (mEta_profile)          mEta_profile         ->Fill(numPV, recoJets[1].eta());
	  if (mPhi_profile)          mPhi_profile         ->Fill(numPV, recoJets[1].phi());
	  if(!isJPTJet_){
	    if (mConstituents_profile) mConstituents_profile->Fill(numPV, recoJets[1].nConstituents());
	  }
	  if(isCaloJet_){
	    if (mHFrac)        mHFrac->Fill ((*caloJets)[ind1].energyFractionHadronic());
	    if (mEFrac)        mEFrac->Fill ((*caloJets)[ind1].emEnergyFraction());
	    if (mMaxEInEmTowers)  mMaxEInEmTowers->Fill ((*caloJets)[ind1].maxEInEmTowers());
	    if (mMaxEInHadTowers) mMaxEInHadTowers->Fill ((*caloJets)[ind1].maxEInHadTowers());
	    if (mHFrac)        mHFrac->Fill ((*caloJets)[ind2].energyFractionHadronic());
	    if (mEFrac)        mEFrac->Fill ((*caloJets)[ind2].emEnergyFraction());
	    if (mMaxEInEmTowers)  mMaxEInEmTowers->Fill ((*caloJets)[ind2].maxEInEmTowers());
	    if (mMaxEInHadTowers) mMaxEInHadTowers->Fill ((*caloJets)[ind2].maxEInHadTowers());  

	    if (mHFrac_profile)        mHFrac_profile       ->Fill(numPV, (*caloJets)[ind1].energyFractionHadronic());
	    if (mEFrac_profile)        mEFrac_profile       ->Fill(numPV, (*caloJets)[ind1].emEnergyFraction());
	    if (mHFrac_profile)        mHFrac_profile       ->Fill(numPV, (*caloJets)[ind2].energyFractionHadronic());
	    if (mEFrac_profile)        mEFrac_profile       ->Fill(numPV, (*caloJets)[ind2].emEnergyFraction());
	  }
	  if(isJPTJet_){
	    if (mHFrac)        mHFrac->Fill ((*jptJets)[ind1].chargedHadronEnergyFraction()+(*jptJets)[ind1].neutralHadronEnergyFraction());
	    //if (mEFrac)        mEFrac->Fill ((*jptJets)[ind1].chargedEmEnergyFraction() +(*jptJets)[ind1].neutralEmEnergyFraction());
	    if (mEFrac)        mEFrac->Fill (1.-(*jptJets)[ind1].chargedHadronEnergyFraction()-(*jptJets)[ind1].neutralHadronEnergyFraction());
	    if (mHFrac_profile)        mHFrac_profile       ->Fill(numPV, (*jptJets)[ind1].chargedHadronEnergyFraction()+(*jptJets)[ind1].neutralHadronEnergyFraction());
	    if (mEFrac_profile)        mEFrac_profile       ->Fill(numPV, 1.-(*jptJets)[ind1].chargedHadronEnergyFraction()-(*jptJets)[ind1].neutralHadronEnergyFraction());
	    if (fabs((*jptJets)[ind1].eta()) <= 1.3) {	
	      if (mHFrac_Barrel)           mHFrac_Barrel->Fill((*jptJets)[ind1].chargedHadronEnergyFraction()+(*jptJets)[ind1].neutralHadronEnergyFraction());	
	      if (mEFrac_Barrel)           mEFrac_Barrel->Fill(1.-(*jptJets)[ind1].chargedHadronEnergyFraction()-(*jptJets)[ind1].neutralHadronEnergyFraction());	
	    }else if(fabs((*jptJets)[ind1].eta()) <3.0){
	      if (mHFrac_EndCap)           mHFrac_EndCap->Fill((*jptJets)[ind1].chargedHadronEnergyFraction()+(*jptJets)[ind1].neutralHadronEnergyFraction());	
	      if (mEFrac_EndCap)           mEFrac_EndCap->Fill(1.-(*jptJets)[ind1].chargedHadronEnergyFraction()-(*jptJets)[ind1].neutralHadronEnergyFraction());
	    }else{
	      if (mHFrac_Forward)           mHFrac_Forward->Fill((*jptJets)[ind1].chargedHadronEnergyFraction()+(*jptJets)[ind1].neutralHadronEnergyFraction());	
	      if (mEFrac_Forward)           mEFrac_Forward->Fill(1.-(*jptJets)[ind1].chargedHadronEnergyFraction()-(*jptJets)[ind1].neutralHadronEnergyFraction());
	    }
	    if (mHFrac)        mHFrac->Fill ((*jptJets)[ind2].chargedHadronEnergyFraction()+(*jptJets)[ind2].neutralHadronEnergyFraction());
	    if (mEFrac)        mEFrac->Fill (1.-(*jptJets)[ind2].chargedHadronEnergyFraction()-(*jptJets)[ind2].neutralHadronEnergyFraction());
	    if (mHFrac_profile)        mHFrac_profile       ->Fill(numPV, (*jptJets)[ind2].chargedHadronEnergyFraction()+(*jptJets)[ind2].neutralHadronEnergyFraction());
	    if (mEFrac_profile)        mEFrac_profile       ->Fill(numPV, 1.-(*jptJets)[ind2].chargedHadronEnergyFraction()-(*jptJets)[ind2].neutralHadronEnergyFraction());
	    if (fabs((*jptJets)[ind2].eta()) <= 1.3) {	
	      if (mHFrac_Barrel)           mHFrac_Barrel->Fill((*jptJets)[ind2].chargedHadronEnergyFraction()+(*jptJets)[ind2].neutralHadronEnergyFraction());	
	      if (mEFrac_Barrel)           mEFrac_Barrel->Fill(1.-(*jptJets)[ind2].chargedHadronEnergyFraction()-(*jptJets)[ind2].neutralHadronEnergyFraction());	
	    }else if(fabs((*jptJets)[ind2].eta()) <3.0){
	      if (mHFrac_EndCap)           mHFrac_EndCap->Fill((*jptJets)[ind2].chargedHadronEnergyFraction()+(*jptJets)[ind2].neutralHadronEnergyFraction());	
	      if (mEFrac_EndCap)           mEFrac_EndCap->Fill(1.-(*jptJets)[ind2].chargedHadronEnergyFraction()-(*jptJets)[ind2].neutralHadronEnergyFraction());
	    }else{
	      if (mHFrac_Forward)           mHFrac_Forward->Fill((*jptJets)[ind2].chargedHadronEnergyFraction()+(*jptJets)[ind2].neutralHadronEnergyFraction());	
	      if (mEFrac_Forward)           mEFrac_Forward->Fill(1.-(*jptJets)[ind2].chargedHadronEnergyFraction()-(*jptJets)[ind2].neutralHadronEnergyFraction());
	    }
	  }
	  if(isPFJet_){
	    if (mHFrac)        mHFrac->Fill ((*pfJets)[ind1].chargedHadronEnergyFraction()+(*pfJets)[ind1].neutralHadronEnergyFraction()+(*pfJets)[ind1].HFHadronEnergyFraction());
	    if (mEFrac)        mEFrac->Fill ((*pfJets)[ind1].chargedEmEnergyFraction() +(*pfJets)[ind1].neutralEmEnergyFraction()+(*pfJets)[ind1].HFEMEnergyFraction());
	    
	    if (mChargedHadronEnergy)  mChargedHadronEnergy->Fill ((*pfJets)[ind1].chargedHadronEnergy());
	    if (mNeutralHadronEnergy)  mNeutralHadronEnergy->Fill ((*pfJets)[ind1].neutralHadronEnergy());
	    if (mChargedEmEnergy) mChargedEmEnergy->Fill((*pfJets)[ind1].chargedEmEnergy());
	    if (mChargedMuEnergy) mChargedMuEnergy->Fill ((*pfJets)[ind1].chargedMuEnergy ());
	    if (mNeutralEmEnergy) mNeutralEmEnergy->Fill((*pfJets)[ind1].neutralEmEnergy());
	    if (mChargedMultiplicity ) mChargedMultiplicity->Fill((*pfJets)[ind1].chargedMultiplicity());
	    if (mNeutralMultiplicity ) mNeutralMultiplicity->Fill((*pfJets)[ind1].neutralMultiplicity());
	    if (mMuonMultiplicity )mMuonMultiplicity->Fill ((*pfJets)[ind1]. muonMultiplicity());
	    //_______________________________________________________
	    if (mNeutralFraction) mNeutralFraction->Fill ((double)(*pfJets)[ind1].neutralMultiplicity()/(double)(*pfJets)[ind1].nConstituents());
	    //Filling variables for second jet
	    if (mHFrac)        mHFrac->Fill ((*pfJets)[ind2].chargedHadronEnergyFraction()+(*pfJets)[ind2].neutralHadronEnergyFraction()+(*pfJets)[ind2].HFHadronEnergyFraction());
	    if (mEFrac)        mEFrac->Fill ((*pfJets)[ind2].chargedEmEnergyFraction() +(*pfJets)[ind2].neutralEmEnergyFraction()+(*pfJets)[ind2].HFEMEnergyFraction());
            
	    if (mChargedHadronEnergy)  mChargedHadronEnergy->Fill ((*pfJets)[ind2].chargedHadronEnergy());
	    if (mNeutralHadronEnergy)  mNeutralHadronEnergy->Fill ((*pfJets)[ind2].neutralHadronEnergy());
	    if (mChargedEmEnergy) mChargedEmEnergy->Fill((*pfJets)[ind2].chargedEmEnergy());
	    if (mChargedMuEnergy) mChargedMuEnergy->Fill ((*pfJets)[ind2].chargedMuEnergy ());
	    if (mNeutralEmEnergy) mNeutralEmEnergy->Fill((*pfJets)[ind2].neutralEmEnergy());
	    if (mChargedMultiplicity ) mChargedMultiplicity->Fill((*pfJets)[ind2].chargedMultiplicity());
	    if (mNeutralMultiplicity ) mNeutralMultiplicity->Fill((*pfJets)[ind2].neutralMultiplicity());
	    if (mMuonMultiplicity )mMuonMultiplicity->Fill ((*pfJets)[ind2]. muonMultiplicity());
	    //_______________________________________________________
	    if (mNeutralFraction) mNeutralFraction->Fill ((double)(*pfJets)[ind2].neutralMultiplicity()/(double)(*pfJets)[ind2].nConstituents());

	    if (mPt_profile)           mPt_profile          ->Fill(numPV, (*pfJets)[ind1].pt());
	    if (mEta_profile)          mEta_profile         ->Fill(numPV, (*pfJets)[ind1].eta());
	    if (mPhi_profile)          mPhi_profile         ->Fill(numPV, (*pfJets)[ind1].phi());
	    if (mConstituents_profile) mConstituents_profile->Fill(numPV, (*pfJets)[ind1].nConstituents());
	    if (mHFrac_profile)        mHFrac_profile       ->Fill(numPV, (*pfJets)[ind1].chargedHadronEnergyFraction() + (*pfJets)[ind1].neutralHadronEnergyFraction());
	    if (mEFrac_profile)        mEFrac_profile       ->Fill(numPV, (*pfJets)[ind1].chargedEmEnergyFraction()     + (*pfJets)[ind1].neutralEmEnergyFraction());
	    
	    if (mChargedHadronEnergy_profile) mChargedHadronEnergy_profile->Fill(numPV, (*pfJets)[ind1].chargedHadronEnergy());
	    if (mNeutralHadronEnergy_profile) mNeutralHadronEnergy_profile->Fill(numPV, (*pfJets)[ind1].neutralHadronEnergy());
	    if (mChargedEmEnergy_profile)     mChargedEmEnergy_profile    ->Fill(numPV, (*pfJets)[ind1].chargedEmEnergy());
	    if (mChargedMuEnergy_profile)     mChargedMuEnergy_profile    ->Fill(numPV, (*pfJets)[ind1].chargedMuEnergy());
	    if (mNeutralEmEnergy_profile)     mNeutralEmEnergy_profile    ->Fill(numPV, (*pfJets)[ind1].neutralEmEnergy());
	    if (mChargedMultiplicity_profile) mChargedMultiplicity_profile->Fill(numPV, (*pfJets)[ind1].chargedMultiplicity());
	    if (mNeutralMultiplicity_profile) mNeutralMultiplicity_profile->Fill(numPV, (*pfJets)[ind1].neutralMultiplicity());
	    if (mMuonMultiplicity_profile)    mMuonMultiplicity_profile   ->Fill(numPV, (*pfJets)[ind1].muonMultiplicity());

	    if (mPt_profile)           mPt_profile          ->Fill(numPV, (*pfJets)[ind2].pt());
	    if (mEta_profile)          mEta_profile         ->Fill(numPV, (*pfJets)[ind2].eta());
	    if (mPhi_profile)          mPhi_profile         ->Fill(numPV, (*pfJets)[ind2].phi());
	    if (mConstituents_profile) mConstituents_profile->Fill(numPV, (*pfJets)[ind2].nConstituents());
	    if (mHFrac_profile)        mHFrac_profile       ->Fill(numPV, (*pfJets)[ind2].chargedHadronEnergyFraction() + (*pfJets)[ind2].neutralHadronEnergyFraction());
	    if (mEFrac_profile)        mEFrac_profile       ->Fill(numPV, (*pfJets)[ind2].chargedEmEnergyFraction()     + (*pfJets)[ind2].neutralEmEnergyFraction());
	    
	    if (mChargedHadronEnergy_profile) mChargedHadronEnergy_profile->Fill(numPV, (*pfJets)[ind2].chargedHadronEnergy());
	    if (mNeutralHadronEnergy_profile) mNeutralHadronEnergy_profile->Fill(numPV, (*pfJets)[ind2].neutralHadronEnergy());
	    if (mChargedEmEnergy_profile)     mChargedEmEnergy_profile    ->Fill(numPV, (*pfJets)[ind2].chargedEmEnergy());
	    if (mChargedMuEnergy_profile)     mChargedMuEnergy_profile    ->Fill(numPV, (*pfJets)[ind2].chargedMuEnergy());
	    if (mNeutralEmEnergy_profile)     mNeutralEmEnergy_profile    ->Fill(numPV, (*pfJets)[ind2].neutralEmEnergy());
	    if (mChargedMultiplicity_profile) mChargedMultiplicity_profile->Fill(numPV, (*pfJets)[ind2].chargedMultiplicity());
	    if (mNeutralMultiplicity_profile) mNeutralMultiplicity_profile->Fill(numPV, (*pfJets)[ind2].neutralMultiplicity());
	    if (mMuonMultiplicity_profile)    mMuonMultiplicity_profile   ->Fill(numPV, (*pfJets)[ind2].muonMultiplicity());
	  }
	  if(fillJIDPassFrac_==1) {
	    if(LoosecleanedFirstJet) {
	      mLooseJIDPassFractionVSeta->Fill(recoJets[0].eta(),1.);
	      mLooseJIDPassFractionVSpt->Fill(recoJets[0].pt(),1.);
	    } else {
	      mLooseJIDPassFractionVSeta->Fill(recoJets[0].eta(),0.);
	      mLooseJIDPassFractionVSpt->Fill(recoJets[0].pt(),0.);
	    }
	    if(LoosecleanedSecondJet) {
	      mLooseJIDPassFractionVSeta->Fill(recoJets[1].eta(),1.);
	      mLooseJIDPassFractionVSpt->Fill(recoJets[1].pt(),1.);
	    } else {
	      mLooseJIDPassFractionVSeta->Fill(recoJets[1].eta(),0.);
	      mLooseJIDPassFractionVSpt->Fill(recoJets[1].pt(),0.);
	    }
         }//if fillJIDPassFrac_
	}//two leading jets loose cleaned
      }//DPhi cut
    }//leading jets within endcaps -> extend to FWD?
    //now do the dijet balance and asymmetry calculations
    if (fabs(recoJets[0].eta() < 1.4)) {
      double pt_dijet = (recoJets[0].pt() + recoJets[1].pt())/2;      
      double dPhiDJ = fabs(recoJets[0].phi()-recoJets[1].phi());
      if(dPhiDJ>acos(-1.)){
	dPhiDJ=2*acos(-1.)-dPhiDJ;
      }
      if (dPhiDJ > 2.7) {
	double pt_probe;
	double pt_barrel;
	int jet1, jet2;
	int randJet = rand() % 2;
	if (fabs(recoJets[1].eta() < 1.4)) {
	  if (randJet) {
	    jet1 = 0;
	    jet2 = 1;
	  }
	  else {
	    jet1 = 1;
	    jet2 = 0;
	  }	  
	  // ***Di-Jet Asymmetry****
	  // * leading jets eta < 1.4
	  // * leading jets dphi > 2.7
	  // * pt_third jet < threshold
	  // * A = (pt_1 - pt_2)/(pt_1 + pt_2)
	  // * jets 1 and two are randomly ordered
	  // **
	  bool thirdJetCut = true;
	  //that doesn't make any sense -> imagine you have 5 jets,
	  //jet 3 is quite hard (almost as hard as the second jet, i.e. 200/80/79/20/15, cutoff is 30
	  //the 4th and 5th jet are soft enough -> then you would fill the asymmetry twice, 
	  //although jet 2 and 3 are basically identical
	  //do third jet relative to second jet
	  //JME-10-014 suggests pt3/pt_dijet<0.15
	  if(ind3>0){
	    if (pt3 > asymmetryThirdJetCut_){
	      thirdJetCut=false;
	    } 
	  }
	  if(thirdJetCut){
	    double dijetAsymmetry =(recoJets[jet1].pt() - recoJets[jet2].pt()) / (recoJets[jet1].pt() + recoJets[jet2].pt());
	    mDijetAsymmetry->Fill(dijetAsymmetry);
	  }// end restriction on third jet pt in asymmetry calculation
	  
	}
	else {
	  jet1 = 0;
	  jet2 = 1;
	}
	
	pt_barrel = recoJets[jet1].pt();
	pt_probe  = recoJets[jet2].pt();
	
	//dijet balance cuts
	// ***Di-Jet Balance****
	// * pt_dijet = (pt_probe+pt_barrel)/2
	// * leading jets dphi > 2.7
	// * reject evnets where pt_third/pt_dijet > 0.2
	// * pv selection
	// * B = (pt_probe - pt_barrel)/pt_dijet
	// * select probe randomly from 2 jets if both leading jets are in the barrel
	bool thirdJetCut = true;
	if(ind3>0){
	  if (pt3/pt_dijet > balanceThirdJetCut_){ 
	    thirdJetCut = false;
	  }
	}
	if (thirdJetCut) {
	  double dijetBalance = (pt_probe - pt_barrel) / pt_dijet;
	  mDijetBalance->Fill(dijetBalance);
	}// end restriction on third jet pt ratio in balance calculation
       
      }// dPhi > 2.7
    }// leading jet eta cut for asymmetry and balance calculations
  }//at least two hard corrected jets
  
}

// ***********************************************************
void JetAnalyzer::endJob(void) {
  
  LogTrace(metname)<<"[JetAnalyzer] Saving the histos";


  //--- Jet


  if(outputMEsInRootFile){
      //dbe_->save(mOutputFile_);
    dbe_->save(mOutputFile_);
  }
  
}


namespace jetAnalysis {
  
  TrackPropagatorToCalo::TrackPropagatorToCalo()
    : magneticField_(NULL),
      propagator_(NULL),
      magneticFieldCacheId_(0),
      propagatorCacheId_(0)
    {}
  
  void TrackPropagatorToCalo::update(const edm::EventSetup& eventSetup)
  {
    //update magnetic filed if necessary
    const IdealMagneticFieldRecord& magneticFieldRecord = eventSetup.get<IdealMagneticFieldRecord>();
    const uint32_t newMagneticFieldCacheId = magneticFieldRecord.cacheIdentifier();
    if ((newMagneticFieldCacheId != magneticFieldCacheId_) || !magneticField_) {
      edm::ESHandle<MagneticField> magneticFieldHandle;
      magneticFieldRecord.get(magneticFieldHandle);
      magneticField_ = magneticFieldHandle.product();
      magneticFieldCacheId_ = newMagneticFieldCacheId;
    }
    //update propagator if necessary
    const TrackingComponentsRecord& trackingComponentsRecord = eventSetup.get<TrackingComponentsRecord>();
    const uint32_t newPropagatorCacheId = trackingComponentsRecord.cacheIdentifier();
    if ((propagatorCacheId_ != newPropagatorCacheId) || !propagator_) {
      edm::ESHandle<Propagator> propagatorHandle;
      trackingComponentsRecord.get("SteppingHelixPropagatorAlong",propagatorHandle);
      propagator_ = propagatorHandle.product();
      propagatorCacheId_ = newPropagatorCacheId;
    }
  }
  
  inline math::XYZPoint TrackPropagatorToCalo::impactPoint(const reco::Track& track) const
  {
    return JetTracksAssociationDRCalo::propagateTrackToCalorimeter(track,*magneticField_,*propagator_);
  }
  /*
  StripSignalOverNoiseCalculator::StripSignalOverNoiseCalculator(const std::string& theQualityLabel)
    : qualityLabel_(theQualityLabel),
      quality_(NULL),
      noise_(NULL),
      gain_(NULL),
      qualityCacheId_(0),
      noiseCacheId_(0),
      gainCacheId_(0)
    {}
  
  void StripSignalOverNoiseCalculator::update(const edm::EventSetup& eventSetup)
  {
    //update the quality if necessary
    const SiStripQualityRcd& qualityRecord = eventSetup.get<SiStripQualityRcd>();
    const uint32_t newQualityCacheId = qualityRecord.cacheIdentifier();
    if ((newQualityCacheId != qualityCacheId_) || !quality_) {
      edm::ESHandle<SiStripQuality> qualityHandle;
      qualityRecord.get(qualityLabel_,qualityHandle);
      quality_ = qualityHandle.product();
      qualityCacheId_ = newQualityCacheId;
    }
    //update the noise if necessary
    const SiStripNoisesRcd& noiseRecord = eventSetup.get<SiStripNoisesRcd>();
    const uint32_t newNoiseCacheId = noiseRecord.cacheIdentifier();
    if ((newNoiseCacheId != noiseCacheId_) || !noise_) {
      edm::ESHandle<SiStripNoises> noiseHandle;
      noiseRecord.get(noiseHandle);
      noise_ = noiseHandle.product();
      noiseCacheId_ = newNoiseCacheId;
    }
    //update the gain if necessary
    const SiStripGainRcd& gainRecord = eventSetup.get<SiStripGainRcd>();
    const uint32_t newGainCacheId = gainRecord.cacheIdentifier();
    if ((newGainCacheId != gainCacheId_) || !gain_) {
      edm::ESHandle<SiStripGain> gainHandle;
      gainRecord.get(gainHandle);
      gain_ = gainHandle.product();
      gainCacheId_ = newGainCacheId;
    }
    }*/
  /*
  double StripSignalOverNoiseCalculator::signalOverNoise(const SiStripCluster& cluster,
							 const uint32_t& detId) const
  {
    //const uint32_t detId = cluster.geographicalId();
    
    const uint16_t firstStrip = cluster.firstStrip();
    const SiStripQuality::Range& qualityRange = quality_->getRange(detId);
    const SiStripNoises::Range& noiseRange = noise_->getRange(detId);
    const SiStripApvGain::Range& gainRange = gain_->getRange(detId);
    double signal = 0;
    double noise2 = 0;
    unsigned int nNonZeroStrips = 0;
    const std::vector<uint8_t>& clusterAmplitudes = cluster.amplitudes();
    const std::vector<uint8_t>::const_iterator clusterAmplitudesEnd = clusterAmplitudes.end();
    const std::vector<uint8_t>::const_iterator clusterAmplitudesBegin = clusterAmplitudes.begin();
    for (std::vector<uint8_t>::const_iterator iAmp = clusterAmplitudesBegin; iAmp != clusterAmplitudesEnd; ++iAmp) {
      const uint8_t adc = *iAmp;
      const uint16_t strip = iAmp-clusterAmplitudesBegin+firstStrip;
      const bool stripBad = quality_->IsStripBad(qualityRange,strip);
      const double noise = noise_->getNoise(strip,noiseRange);
      const double gain = gain_->getStripGain(strip,gainRange);
      signal += adc;
      if (adc) ++nNonZeroStrips;
      const double noiseContrib = (stripBad ? 0 : noise/gain);
      noise2 += noiseContrib*noiseContrib;
    }
    const double noise = sqrt(noise2/nNonZeroStrips);
    if (noise) return signal/noise;
    else return 0;
  }
  */ 
}

