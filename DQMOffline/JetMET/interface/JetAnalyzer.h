#ifndef JetAnalyzer_H
#define JetAnalyzer_H


/** \class JetMETAnalyzer
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


#include <memory>
#include <fstream>
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "DQMServices/Core/interface/DQMStore.h"
#include "DQMServices/Core/interface/MonitorElement.h"

#include "DataFormats/Common/interface/TriggerResults.h"
#include "DataFormats/HLTReco/interface/TriggerEvent.h"
#include "DataFormats/HLTReco/interface/TriggerObject.h"
#include "DataFormats/HLTReco/interface/TriggerTypeDefs.h"
#include "HLTrigger/HLTcore/interface/HLTConfigProvider.h"
#include "CommonTools/TriggerUtils/interface/GenericTriggerEventFlag.h"
#include "FWCore/Utilities/interface/EDGetToken.h"

#include "DataFormats/JetReco/interface/Jet.h"
#include "DataFormats/JetReco/interface/CaloJet.h"
#include "DataFormats/JetReco/interface/CaloJetCollection.h"
#include "DataFormats/JetReco/interface/GenJet.h"
#include "DataFormats/JetReco/interface/GenJetCollection.h"
#include "DataFormats/JetReco/interface/JPTJet.h"
#include "DataFormats/JetReco/interface/JPTJetCollection.h"
#include "DataFormats/JetReco/interface/PFJet.h"
#include "JetMETCorrections/Objects/interface/JetCorrector.h"
#include "RecoJets/JetProducers/interface/JetIDHelper.h"

#include "DQMOffline/JetMET/interface/JetMETDQMDCSFilter.h"

#include "DataFormats/VertexReco/interface/Vertex.h"
#include "DataFormats/VertexReco/interface/VertexFwd.h"

#include "DataFormats/Scalers/interface/DcsStatus.h" 
#include "PhysicsTools/SelectorUtils/interface/JetIDSelectionFunctor.h"
#include "PhysicsTools/SelectorUtils/interface/PFJetIDSelectionFunctor.h"

#include "DataFormats/PatCandidates/interface/Jet.h"

namespace jetAnalysis {
  class TrackPropagatorToCalo;
  //class StripSignalOverNoiseCalculator;
}

class JetAnalyzer : public edm::EDAnalyzer {
 public:

  /// Constructor
  JetAnalyzer(const edm::ParameterSet&);
  
  /// Destructor
  virtual ~JetAnalyzer();
  
  /// Inizialize parameters for histo binning
  void beginJob(void);

  /// Get the analysis
  void analyze(const edm::Event&, const edm::EventSetup&);

  /// Save the histos
  void endJob(void);

  /// Initialize run-based parameters
  void beginRun(const edm::Run&,  const edm::EventSetup&);

  /// Finish up a run
  void endRun(const edm::Run&,  const edm::EventSetup&);


 private:
  // ----------member data ---------------------------
  static bool jetSortingRule(reco::Jet x, reco::Jet y) {return x.pt() > y.pt();}


 /// Helper object to propagate tracks to the calo surface
  std::auto_ptr<jetAnalysis::TrackPropagatorToCalo> trackPropagator_;
  /// Helper object to calculate strip SoN for tracks
  //std::auto_ptr<jetAnalysis::StripSignalOverNoiseCalculator> sOverNCalculator_;

  /// Helper object to propagate tracks to the calo surface
  //jetAnalysis::TrackPropagatorToCalo trackPropagator_;
  /// Helper object to calculate strip SoN for tracks
  //jetAnalysis::StripSignalOverNoiseCalculator sOverNCalculator_;

  DQMStore* dbe_;

  //try to put one collection as start
  edm::InputTag mInputCollection_;
  edm::InputTag theTriggerResultsLabel_;

  std::string  jetType_;
  std::string  mOutputFile_;

  edm::EDGetTokenT<edm::TriggerResults>           triggerResultsToken_;
  edm::EDGetTokenT<std::vector<reco::Vertex>>     vertexToken_;
  edm::EDGetTokenT<L1GlobalTriggerReadoutRecord>  gtToken_;
  edm::EDGetTokenT<reco::CaloJetCollection>       caloJetsToken_;
  edm::EDGetTokenT<reco::PFJetCollection>         pfJetsToken_;
  edm::EDGetTokenT<reco::JPTJetCollection>        jptJetsToken_;

  edm::InputTag inputJetIDValueMap;
  edm::EDGetTokenT<edm::ValueMap <reco::JetID> >jetID_ValueMapToken_;

  //Cleaning parameters
  edm::ParameterSet cleaningParameters_;
  edm::InputTag vertexLabel_;
  edm::InputTag gtLabel_;

  //check later if we need only one set of parameters
  edm::ParameterSet parameters_;

  std::string jetCorrectionService_;
  std::string JetIDQuality_;
  std::string JetIDVersion_;
  JetIDSelectionFunctor::Quality_t jetidquality;
  JetIDSelectionFunctor::Version_t jetidversion;
  JetIDSelectionFunctor jetIDFunctor;
  JetIDSelectionFunctor jetIDFunctorLoose;
  JetIDSelectionFunctor jetIDFunctorTight;

  PFJetIDSelectionFunctor::Quality_t pfjetidquality;
  PFJetIDSelectionFunctor::Version_t pfjetidversion;

  PFJetIDSelectionFunctor pfjetIDFunctor;
  PFJetIDSelectionFunctor pfjetIDFunctorLoose;
  PFJetIDSelectionFunctor pfjetIDFunctorTight;

  pat::strbitset ret;
  pat::strbitset retLoose;
  pat::strbitset retTight;

  //histo binning parameters -> these are PART of ALL analyzers - move it up
  int    etaBin_;
  double etaMin_;
  double etaMax_;

  int    phiBin_;
  double phiMin_;
  double phiMax_;

  int    ptBin_;
  double ptMin_;
  double ptMax_;

  int    eBin_;
  double eMin_;
  double eMax_;

  int    pBin_;
  double pMin_;
  double pMax_;

  int     nbinsPV_;
  double  nPVlow_; 
  double  nPVhigh_;

  //variables which are present both in 
  int    jetLoPass_;
  int    jetHiPass_;
  int    leadJetFlag_;
  double ptThreshold_;
  double ptThresholdUnc_;
  double asymmetryThirdJetCut_;
  double balanceThirdJetCut_;

  int makedijetselection_;
  //
  int fillJIDPassFrac_;

  //the histos
  MonitorElement* jetME;

  // --- Used for Data Certification - use for Calo, PF and JPT jets
  MonitorElement* mPt;
  MonitorElement* mPt_1;
  MonitorElement* mPt_2;
  MonitorElement* mPt_3;
  MonitorElement* mEta;
  MonitorElement* mPhi;
  MonitorElement* mPt_uncor;
  MonitorElement* mEta_uncor;
  MonitorElement* mPhi_uncor;
  MonitorElement* mConstituents_uncor;

  MonitorElement* mJetEnergyCorr;
  MonitorElement* mJetEnergyCorrVsEta;

  MonitorElement* mConstituents;
  MonitorElement* mHFrac;
  MonitorElement* mEFrac;
  MonitorElement* mPhiVSEta;

  MonitorElement* mPt_Barrel;
  MonitorElement* mPhi_Barrel;
  MonitorElement* mConstituents_Barrel;
  MonitorElement* mHFrac_Barrel;
  MonitorElement* mEFrac_Barrel;

  MonitorElement* mPt_EndCap;
  MonitorElement* mPhi_EndCap;
  MonitorElement* mConstituents_EndCap;
  MonitorElement* mHFrac_EndCap;
  MonitorElement* mEFrac_EndCap;

  MonitorElement* mPt_Forward;
  MonitorElement* mPhi_Forward;
  MonitorElement* mConstituents_Forward;
  MonitorElement* mHFrac_Forward;
  MonitorElement* mEFrac_Forward;


  MonitorElement* mPt_Barrel_Hi;
  MonitorElement* mPhi_Barrel_Hi;
  MonitorElement* mConstituents_Barrel_Hi;
  MonitorElement* mHFrac_Barrel_Hi;

  MonitorElement* mPt_EndCap_Hi;
  MonitorElement* mPhi_EndCap_Hi;
  MonitorElement* mConstituents_EndCap_Hi;
  MonitorElement* mHFrac_EndCap_Hi;

  MonitorElement* mPt_Forward_Hi;
  MonitorElement* mPhi_Forward_Hi;
  MonitorElement* mConstituents_Forward_Hi;
  MonitorElement* mHFrac_Forward_Hi;

  MonitorElement* mNJets;
  MonitorElement* mDPhi;

  // Leading Jet Parameters
  MonitorElement* mEtaFirst;
  MonitorElement* mPhiFirst;
  MonitorElement* mPtFirst;

 // Events passing the jet triggers
  MonitorElement* mPhi_Lo;
  MonitorElement* mPt_Lo;

  MonitorElement* mEta_Hi;
  MonitorElement* mPhi_Hi;
  MonitorElement* mPt_Hi;

  MonitorElement* mLooseJIDPassFractionVSeta;
  MonitorElement* mLooseJIDPassFractionVSpt;
  MonitorElement* mTightJIDPassFractionVSeta;
  MonitorElement* mTightJIDPassFractionVSpt;

  //dijet analysis quantities
  MonitorElement* mDijetBalance;
  MonitorElement* mDijetAsymmetry;

  // NPV profiles
  //----------------------------------------------------------------------------
  MonitorElement* mNJets_profile;
  MonitorElement* mPt_profile;
  MonitorElement* mEta_profile;
  MonitorElement* mPhi_profile;
  MonitorElement* mConstituents_profile;
  MonitorElement* mHFrac_profile;
  MonitorElement* mEFrac_profile;

  bool hltInitialized_;
  bool doPVCheck_;

  int     nvtxMin_;
  int     vtxNdofMin_;
  double  vtxZMax_;

  int LSBegin_;
  int LSEnd_;

  HLTConfigProvider hltConfig_;
  std::string processname_;

  //MonitorElement* hltpathME;
  MonitorElement* lumisecME;
  MonitorElement* cleanupME;
  MonitorElement* verticesME;

  GenericTriggerEventFlag * highPtJetEventFlag_;
  GenericTriggerEventFlag * lowPtJetEventFlag_;

  std::vector<std::string> highPtJetExpr_;
  std::vector<std::string> lowPtJetExpr_;


  bool diJetSelectionFlag_;
  bool jetCleaningFlag_;

//  bool energycorrected;
 

  // CaloJet specific
  MonitorElement* mMaxEInEmTowers;
  MonitorElement* mMaxEInHadTowers;
  MonitorElement* mHadEnergyInHO;
  MonitorElement* mHadEnergyInHB;
  MonitorElement* mHadEnergyInHF;
  MonitorElement* mHadEnergyInHE;
  MonitorElement* mEmEnergyInEB;
  MonitorElement* mEmEnergyInEE;
  MonitorElement* mEmEnergyInHF;
  MonitorElement* mN90Hits;
  MonitorElement* mfHPD;
  MonitorElement* mfRBX;
  MonitorElement* mresEMF;
  MonitorElement* mEMF;
 
  // JPTJet specific
  // the jet analyzer
  // --- Used for Data Certification --in for CaloJets and PFJets
  MonitorElement* mE;
  MonitorElement* mP;
  MonitorElement* mEt;
  MonitorElement* mPtSecond;
  MonitorElement* mPtThird;
  MonitorElement* mPx;
  MonitorElement* mPy;
  MonitorElement* mPz;
  MonitorElement* mnTracks;
  MonitorElement* mnTracksVsJetPt;
  MonitorElement* mnTracksVsJetEta;

  MonitorElement* mnallPionsTracksPerJet;
  MonitorElement* mallPionsTracksPt;
  MonitorElement* mallPionsTracksPhi;
  MonitorElement* mallPionsTracksEta;
  MonitorElement* mallPionsTracksPtVsEta;

  MonitorElement* mnInVertexInCaloPionsTracksPerJet;
  MonitorElement* mInVertexInCaloPionsTracksPt;
  MonitorElement* mInVertexInCaloPionsTracksPhi;
  MonitorElement* mInVertexInCaloPionsTracksEta;
  MonitorElement* mInVertexInCaloPionsTracksPtVsEta;

  MonitorElement* mnOutVertexInCaloPionsTracksPerJet;
  MonitorElement* mOutVertexInCaloPionsTracksPt;
  MonitorElement* mOutVertexInCaloPionsTracksPhi;
  MonitorElement* mOutVertexInCaloPionsTracksEta;
  MonitorElement* mOutVertexInCaloPionsTracksPtVsEta;

  MonitorElement* mnInVertexOutCaloPionsTracksPerJet;
  MonitorElement* mInVertexOutCaloPionsTracksPt;
  MonitorElement* mInVertexOutCaloPionsTracksPhi;
  MonitorElement* mInVertexOutCaloPionsTracksEta;
  MonitorElement* mInVertexOutCaloPionsTracksPtVsEta;

  MonitorElement* mnallMuonsTracksPerJet;
  MonitorElement* mallMuonsTracksPt;
  MonitorElement* mallMuonsTracksPhi;
  MonitorElement* mallMuonsTracksEta;
  MonitorElement* mallMuonsTracksPtVsEta;

  MonitorElement* mnInVertexInCaloMuonsTracksPerJet;
  MonitorElement* mInVertexInCaloMuonsTracksPt;
  MonitorElement* mInVertexInCaloMuonsTracksPhi;
  MonitorElement* mInVertexInCaloMuonsTracksEta;
  MonitorElement* mInVertexInCaloMuonsTracksPtVsEta;

  MonitorElement* mnOutVertexInCaloMuonsTracksPerJet;
  MonitorElement* mOutVertexInCaloMuonsTracksPt;
  MonitorElement* mOutVertexInCaloMuonsTracksPhi;
  MonitorElement* mOutVertexInCaloMuonsTracksEta;
  MonitorElement* mOutVertexInCaloMuonsTracksPtVsEta;

  MonitorElement* mnInVertexOutCaloMuonsTracksPerJet;
  MonitorElement* mInVertexOutCaloMuonsTracksPt;
  MonitorElement* mInVertexOutCaloMuonsTracksPhi;
  MonitorElement* mInVertexOutCaloMuonsTracksEta;
  MonitorElement* mInVertexOutCaloMuonsTracksPtVsEta;

  MonitorElement* mnallElectronsTracksPerJet;
  MonitorElement* mallElectronsTracksPt;
  MonitorElement* mallElectronsTracksPhi;
  MonitorElement* mallElectronsTracksEta;
  MonitorElement* mallElectronsTracksPtVsEta;

  MonitorElement* mnInVertexInCaloElectronsTracksPerJet;
  MonitorElement* mInVertexInCaloElectronsTracksPt;
  MonitorElement* mInVertexInCaloElectronsTracksPhi;
  MonitorElement* mInVertexInCaloElectronsTracksEta;
  MonitorElement* mInVertexInCaloElectronsTracksPtVsEta;

  MonitorElement* mnOutVertexInCaloElectronsTracksPerJet;
  MonitorElement* mOutVertexInCaloElectronsTracksPt;
  MonitorElement* mOutVertexInCaloElectronsTracksPhi;
  MonitorElement* mOutVertexInCaloElectronsTracksEta;
  MonitorElement* mOutVertexInCaloElectronsTracksPtVsEta;

  MonitorElement* mnInVertexOutCaloElectronsTracksPerJet;
  MonitorElement* mInVertexOutCaloElectronsTracksPt;
  MonitorElement* mInVertexOutCaloElectronsTracksPhi;
  MonitorElement* mInVertexOutCaloElectronsTracksEta;
  MonitorElement* mInVertexOutCaloElectronsTracksPtVsEta;

  MonitorElement* mInCaloTrackDirectionJetDRHisto_;
  MonitorElement* mOutCaloTrackDirectionJetDRHisto_;
  MonitorElement* mInVertexTrackImpactPointJetDRHisto_;
  MonitorElement* mOutVertexTrackImpactPointJetDRHisto_;

  //now define PFJet only flags
  double thisCHFMin_;
  double thisNHFMax_;
  double thisCEFMax_;
  double thisNEFMax_;
  double looseCHFMin_;
  double looseNHFMax_;
  double looseCEFMax_;
  double looseNEFMax_;
  double tightCHFMin_;
  double tightNHFMax_;
  double tightCEFMax_;
  double tightNEFMax_;

  MonitorElement* mCHFrac_lowPt_Barrel;
  MonitorElement* mNHFrac_lowPt_Barrel;
  MonitorElement* mPhFrac_lowPt_Barrel;
  MonitorElement* mElFrac_lowPt_Barrel;
  MonitorElement* mMuFrac_lowPt_Barrel;
  MonitorElement* mCHFrac_mediumPt_Barrel;
  MonitorElement* mNHFrac_mediumPt_Barrel;
  MonitorElement* mPhFrac_mediumPt_Barrel;
  MonitorElement* mElFrac_mediumPt_Barrel;
  MonitorElement* mMuFrac_mediumPt_Barrel;
  MonitorElement* mCHFrac_highPt_Barrel;
  MonitorElement* mNHFrac_highPt_Barrel;
  MonitorElement* mPhFrac_highPt_Barrel;
  MonitorElement* mElFrac_highPt_Barrel;
  MonitorElement* mMuFrac_highPt_Barrel;
  MonitorElement* mCHEn_lowPt_Barrel;
  MonitorElement* mNHEn_lowPt_Barrel;
  MonitorElement* mPhEn_lowPt_Barrel;
  MonitorElement* mElEn_lowPt_Barrel;
  MonitorElement* mMuEn_lowPt_Barrel;
  MonitorElement* mCHEn_mediumPt_Barrel;
  MonitorElement* mNHEn_mediumPt_Barrel;
  MonitorElement* mPhEn_mediumPt_Barrel;
  MonitorElement* mElEn_mediumPt_Barrel;
  MonitorElement* mMuEn_mediumPt_Barrel;
  MonitorElement* mCHEn_highPt_Barrel;
  MonitorElement* mNHEn_highPt_Barrel;
  MonitorElement* mPhEn_highPt_Barrel;
  MonitorElement* mElEn_highPt_Barrel;
  MonitorElement* mMuEn_highPt_Barrel;
  MonitorElement* mChMultiplicity_lowPt_Barrel;
  MonitorElement* mNeuMultiplicity_lowPt_Barrel;
  MonitorElement* mMuMultiplicity_lowPt_Barrel;
  MonitorElement* mChMultiplicity_mediumPt_Barrel;
  MonitorElement* mNeuMultiplicity_mediumPt_Barrel;
  MonitorElement* mMuMultiplicity_mediumPt_Barrel;
  MonitorElement* mChMultiplicity_highPt_Barrel;
  MonitorElement* mNeuMultiplicity_highPt_Barrel;
  MonitorElement* mMuMultiplicity_highPt_Barrel;

  MonitorElement*  mCHFracVSpT_Barrel;
  MonitorElement*  mNHFracVSpT_Barrel;
  MonitorElement*  mPhFracVSpT_Barrel;
  MonitorElement*  mElFracVSpT_Barrel;
  MonitorElement*  mMuFracVSpT_Barrel;
  MonitorElement*  mCHFracVSpT_EndCap;
  MonitorElement*  mNHFracVSpT_EndCap;
  MonitorElement*  mPhFracVSpT_EndCap;
  MonitorElement*  mElFracVSpT_EndCap;
  MonitorElement*  mMuFracVSpT_EndCap;
  MonitorElement*  mHFHFracVSpT_Forward;
  MonitorElement*  mHFEFracVSpT_Forward;

  MonitorElement*  mCHFracVSeta_lowPt;
  MonitorElement*  mNHFracVSeta_lowPt;
  MonitorElement*  mPhFracVSeta_lowPt;
  MonitorElement*  mElFracVSeta_lowPt;
  MonitorElement*  mMuFracVSeta_lowPt;
  MonitorElement*  mCHFracVSeta_mediumPt;
  MonitorElement*  mNHFracVSeta_mediumPt;
  MonitorElement*  mPhFracVSeta_mediumPt;
  MonitorElement*  mElFracVSeta_mediumPt;
  MonitorElement*  mMuFracVSeta_mediumPt;
  MonitorElement*  mCHFracVSeta_highPt;
  MonitorElement*  mNHFracVSeta_highPt;
  MonitorElement*  mPhFracVSeta_highPt;
  MonitorElement*  mElFracVSeta_highPt;
  MonitorElement*  mMuFracVSeta_highPt;

  MonitorElement* mCHFrac_lowPt_EndCap;
  MonitorElement* mNHFrac_lowPt_EndCap;
  MonitorElement* mPhFrac_lowPt_EndCap;
  MonitorElement* mElFrac_lowPt_EndCap;
  MonitorElement* mMuFrac_lowPt_EndCap;
  MonitorElement* mCHFrac_mediumPt_EndCap;
  MonitorElement* mNHFrac_mediumPt_EndCap;
  MonitorElement* mPhFrac_mediumPt_EndCap;
  MonitorElement* mElFrac_mediumPt_EndCap;
  MonitorElement* mMuFrac_mediumPt_EndCap;
  MonitorElement* mCHFrac_highPt_EndCap;
  MonitorElement* mNHFrac_highPt_EndCap;
  MonitorElement* mPhFrac_highPt_EndCap;
  MonitorElement* mElFrac_highPt_EndCap;
  MonitorElement* mMuFrac_highPt_EndCap;

  MonitorElement* mCHEn_lowPt_EndCap;
  MonitorElement* mNHEn_lowPt_EndCap;
  MonitorElement* mPhEn_lowPt_EndCap;
  MonitorElement* mElEn_lowPt_EndCap;
  MonitorElement* mMuEn_lowPt_EndCap;
  MonitorElement* mCHEn_mediumPt_EndCap;
  MonitorElement* mNHEn_mediumPt_EndCap;
  MonitorElement* mPhEn_mediumPt_EndCap;
  MonitorElement* mElEn_mediumPt_EndCap;
  MonitorElement* mMuEn_mediumPt_EndCap;
  MonitorElement* mCHEn_highPt_EndCap;
  MonitorElement* mNHEn_highPt_EndCap;
  MonitorElement* mPhEn_highPt_EndCap;
  MonitorElement* mElEn_highPt_EndCap;
  MonitorElement* mMuEn_highPt_EndCap;

  MonitorElement*   mChMultiplicity_lowPt_EndCap;
  MonitorElement*   mNeuMultiplicity_lowPt_EndCap;
  MonitorElement*   mMuMultiplicity_lowPt_EndCap;
  MonitorElement*   mChMultiplicity_mediumPt_EndCap;
  MonitorElement*   mNeuMultiplicity_mediumPt_EndCap;
  MonitorElement*   mMuMultiplicity_mediumPt_EndCap;
  MonitorElement*   mChMultiplicity_highPt_EndCap;
  MonitorElement*   mNeuMultiplicity_highPt_EndCap;
  MonitorElement*   mMuMultiplicity_highPt_EndCap;


  MonitorElement*   mHFEFrac_lowPt_Forward;
  MonitorElement*   mHFHFrac_lowPt_Forward;
  MonitorElement*   mHFEFrac_mediumPt_Forward;
  MonitorElement*   mHFHFrac_mediumPt_Forward;
  MonitorElement*   mHFEFrac_highPt_Forward;
  MonitorElement*   mHFHFrac_highPt_Forward;
  MonitorElement*   mHFEEn_lowPt_Forward;
  MonitorElement*   mHFHEn_lowPt_Forward;
  MonitorElement*   mHFEEn_mediumPt_Forward;
  MonitorElement*   mHFHEn_mediumPt_Forward;
  MonitorElement*   mHFEEn_highPt_Forward;
  MonitorElement*   mHFHEn_highPt_Forward;
  MonitorElement*   mChMultiplicity_lowPt_Forward;
  MonitorElement*   mNeuMultiplicity_lowPt_Forward;
  MonitorElement*   mMuMultiplicity_lowPt_Forward;
  MonitorElement*   mChMultiplicity_mediumPt_Forward;
  MonitorElement*   mNeuMultiplicity_mediumPt_Forward;
  MonitorElement*   mMuMultiplicity_mediumPt_Forward;
  MonitorElement*   mChMultiplicity_highPt_Forward;
  MonitorElement*   mNeuMultiplicity_highPt_Forward;
  MonitorElement*   mMuMultiplicity_highPt_Forward;

  MonitorElement* mChargedHadronEnergy;
  MonitorElement* mNeutralHadronEnergy;
  MonitorElement* mChargedEmEnergy;
  MonitorElement* mChargedMuEnergy;
  MonitorElement* mNeutralEmEnergy;
  MonitorElement* mChargedMultiplicity;
  MonitorElement* mNeutralMultiplicity;
  MonitorElement* mMuonMultiplicity;

  //new Plots with Res./ Eff. as function of neutral, charged &  em fraction

  MonitorElement* mNeutralFraction;
  MonitorElement* mNeutralFraction2;

  MonitorElement* mEEffNeutralFraction;
  MonitorElement* mEEffChargedFraction;
  MonitorElement* mEResNeutralFraction;
  MonitorElement* mEResChargedFraction;
  MonitorElement* nEEff;
  //PF specific NPV profiles
  MonitorElement* mChargedHadronEnergy_profile;
  MonitorElement* mNeutralHadronEnergy_profile;
  MonitorElement* mChargedEmEnergy_profile;
  MonitorElement* mChargedMuEnergy_profile;
  MonitorElement* mNeutralEmEnergy_profile;
  MonitorElement* mChargedMultiplicity_profile;
  MonitorElement* mNeutralMultiplicity_profile;
  MonitorElement* mMuonMultiplicity_profile;

  JetMETDQMDCSFilter * DCSFilterForJetMonitoring_;
  JetMETDQMDCSFilter * DCSFilterForDCSMonitoring_;

  bool isCaloJet_;
  bool isJPTJet_;
  bool isPFJet_;

};
#endif  
