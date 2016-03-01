/** \class PFCandidateAnalyzer
 *
 *  DQM jetMET analysis monitoring
 *  for PFCandidates
 *
 *          Jan. '16: by
 *
 *          M. Artur Weber
 */

#include "DQMOffline/JetMET/interface/PFCandidateAnalyzer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Common/interface/TriggerNames.h"
#include "DataFormats/JetReco/interface/CaloJetCollection.h"
#include "DataFormats/JetReco/interface/CaloJet.h"
#include "DataFormats/JetReco/interface/JPTJetCollection.h"
#include "DataFormats/JetReco/interface/PFJetCollection.h"
#include "DataFormats/JetReco/interface/PFJet.h"
#include "DataFormats/PatCandidates/interface/Jet.h"
#include "DataFormats/Candidate/interface/Candidate.h"
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

#include <cmath>

using namespace edm;
using namespace reco;
using namespace std;

// ***********************************************************
PFCandidateAnalyzer::PFCandidateAnalyzer(const edm::ParameterSet& pSet)
//: trackPropagator_(new jetAnalysis::TrackPropagatorToCalo)//,
    //sOverNCalculator_(new jetAnalysis::StripSignalOverNoiseCalculator)
{

  miniaodfilterdec=-1;

  candidateType_= pSet.getUntrackedParameter<std::string>("CandType");
  //here only choice between miniaod or reco

  isMiniAOD_= (std::string("Packed") == candidateType_);

  mInputCollection_       = pSet.getParameter<edm::InputTag>("PFCandidateLabel");

  if(isMiniAOD_){
    pflowPackedToken_ = consumes<std::vector<pat::PackedCandidate> >(mInputCollection_);
  }else{
    pflowToken_ = consumes<std::vector<reco::PFCandidate> >(mInputCollection_);
  }
  
  miniaodfilterdec=-1;

 // Smallest track pt
  ptMinCand_ = pSet.getParameter<double>("ptMinCand");

  // Smallest raw HCAL energy linked to the track
  hcalMin_ = pSet.getParameter<double>("hcalMin");

  diagnosticsParameters_ = pSet.getParameter<std::vector<edm::ParameterSet> >("METDiagonisticsParameters");

  edm::ConsumesCollector iC  = consumesCollector();
  //DCS
  DCSFilter_ = new JetMETDQMDCSFilter(pSet.getParameter<ParameterSet>("DCSFilter"), iC );

  METFilterMiniAODLabel_=pSet.getParameter<edm::InputTag>("FilterResultsLabelMiniAOD");
  METFilterMiniAODToken_=consumes<edm::TriggerResults>(METFilterMiniAODLabel_);

  METFilterMiniAODLabel2_=pSet.getParameter<edm::InputTag>("FilterResultsLabelMiniAOD2");
  METFilterMiniAODToken2_=consumes<edm::TriggerResults>(METFilterMiniAODLabel2_);


  HBHENoiseStringMiniAOD = pSet.getParameter<std::string>("HBHENoiseLabelMiniAOD");

  hbheNoiseFilterResultTag_    = pSet.getParameter<edm::InputTag>("HBHENoiseFilterResultLabel");
  hbheNoiseFilterResultToken_=consumes<bool>(hbheNoiseFilterResultTag_);
  
  //jet cleanup parameters
  cleaningParameters_ = pSet.getParameter<ParameterSet>("CleaningParameters");

  //Vertex requirements
  bypassAllPVChecks_    = cleaningParameters_.getParameter<bool>("bypassAllPVChecks");
  bypassAllDCSChecks_    = cleaningParameters_.getParameter<bool>("bypassAllDCSChecks");
  vertexTag_    = cleaningParameters_.getParameter<edm::InputTag>("vertexCollection");
  vertexToken_  = consumes<std::vector<reco::Vertex> >(edm::InputTag(vertexTag_));
  
  verbose_= pSet.getParameter<int>("verbose");
}  
  

// ***********************************************************
PFCandidateAnalyzer::~PFCandidateAnalyzer() {

  delete DCSFilter_;
  LogTrace("PFCandidateAnalyzer")<<"[PFCandidateAnalyzer] Saving the histos";
}

// ***********************************************************
void PFCandidateAnalyzer::bookHistograms(DQMStore::IBooker & ibooker,
				     edm::Run const & iRun,
				     edm::EventSetup const & ) {

  ibooker.setCurrentFolder("JetMET/PFCandidates/"+mInputCollection_.label());
  std::string DirName = "JetMET/PFCandidates/"+mInputCollection_.label();

  if(!isMiniAOD_){
    if(!etaMinPFCand_.empty()){
      etaMinPFCand_.clear();
      etaMaxPFCand_.clear();
      typePFCand_.clear();
      countsPFCand_.clear();
      occupancyPFCand_.clear();
      ptPFCand_.clear();
      multiplicityPFCand_.clear();
      occupancyPFCand_name_.clear();
      ptPFCand_name_.clear();
      multiplicityPFCand_name_.clear();
    }
    for (std::vector<edm::ParameterSet>::const_iterator v = diagnosticsParameters_.begin(); v!=diagnosticsParameters_.end(); v++) {
      int etaNBinsPFCand = v->getParameter<int>("etaNBins");
      double etaMinPFCand = v->getParameter<double>("etaMin");
      double etaMaxPFCand = v->getParameter<double>("etaMax");
      int phiNBinsPFCand = v->getParameter<int>("phiNBins");
      double phiMinPFCand = v->getParameter<double>("phiMin");
      double phiMaxPFCand = v->getParameter<double>("phiMax");
      int nMinPFCand = v->getParameter<int>("nMin");
      int nMaxPFCand = v->getParameter<int>("nMax");
      int nbinsPFCand = v->getParameter<double>("nbins");      
      etaMinPFCand_.push_back(etaMinPFCand);
      etaMaxPFCand_.push_back(etaMaxPFCand);
      typePFCand_.push_back(v->getParameter<int>("type"));
      countsPFCand_.push_back(0);
      multiplicityPFCand_.push_back(ibooker.book1D(std::string(v->getParameter<std::string>("name")).append("_multiplicity_").c_str(),std::string(v->getParameter<std::string>("name"))+"multiplicity", nbinsPFCand, nMinPFCand, nMaxPFCand));
      multiplicityPFCand_name_.push_back(std::string(v->getParameter<std::string>("name")).append("_multiplicity_").c_str());
      map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+ multiplicityPFCand_name_[multiplicityPFCand_name_.size()-1], multiplicityPFCand_[multiplicityPFCand_.size()-1]));
      
      //push back names first, we need to create histograms with the name and fill it for endcap plots later
      occupancyPFCand_name_.push_back(std::string(v->getParameter<std::string>("name")).append("_occupancy_").c_str());
      
      ptPFCand_name_.push_back(std::string(v->getParameter<std::string>("name")).append("_pt_").c_str());
      //std::cout<<"define histo "<<ptPFCand_name_[ptPFCand_name_.size()-1]<<std::endl;
      //special booking for endcap plots, merge plots for eminus and eplus into one plot, using variable binning
      //barrel plots have eta-boundaries on minus and plus side
      //parameters start on minus side
      if(etaMinPFCand*etaMaxPFCand<0){//barrel plots, plot only in barrel region
	occupancyPFCand_.push_back(ibooker.book2D(std::string(v->getParameter<std::string>("name")).append("_occupancy_").c_str(),std::string(v->getParameter<std::string>("name"))+"occupancy", etaNBinsPFCand, etaMinPFCand, etaMaxPFCand, phiNBinsPFCand, phiMinPFCand, phiMaxPFCand));
	ptPFCand_.push_back(ibooker.book2D(std::string(v->getParameter<std::string>("name")).append("_pt_").c_str(),std::string(v->getParameter<std::string>("name"))+"pt", etaNBinsPFCand, etaMinPFCand, etaMaxPFCand, phiNBinsPFCand, phiMinPFCand, phiMaxPFCand));
      }else{//endcap or forward plots, 
	const int nbins_eta_endcap=2*(etaNBinsPFCand+1);
	double eta_limits_endcap[nbins_eta_endcap];
	for(int i=0;i<nbins_eta_endcap;i++){
	  if(i<(etaNBinsPFCand+1)){
	    eta_limits_endcap[i]=etaMinPFCand+i*(etaMaxPFCand-etaMinPFCand)/(double)etaNBinsPFCand;
	  }else{
	    eta_limits_endcap[i]= -etaMaxPFCand +(i- (etaNBinsPFCand+1) )*(etaMaxPFCand-etaMinPFCand)/(double)etaNBinsPFCand;
	  }
	}
	TH2F* hist_temp_occup = new TH2F((occupancyPFCand_name_[occupancyPFCand_name_.size()-1]).c_str(),"occupancy",nbins_eta_endcap-1, eta_limits_endcap, phiNBinsPFCand, phiMinPFCand, phiMaxPFCand);
	occupancyPFCand_.push_back(ibooker.book2D(occupancyPFCand_name_[occupancyPFCand_name_.size()-1],hist_temp_occup));
	TH2F* hist_temp_pt = new TH2F((ptPFCand_name_[ptPFCand_name_.size()-1]).c_str(),"pt",nbins_eta_endcap-1, eta_limits_endcap, phiNBinsPFCand, phiMinPFCand, phiMaxPFCand);
	ptPFCand_.push_back(ibooker.book2D(ptPFCand_name_[ptPFCand_name_.size()-1], hist_temp_pt));
      }
      
      map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+ occupancyPFCand_name_[occupancyPFCand_name_.size()-1], occupancyPFCand_[occupancyPFCand_.size()-1]));
      map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+ ptPFCand_name_[ptPFCand_name_.size()-1], ptPFCand_[ptPFCand_.size()-1]));
    }
    
    mProfileIsoPFChHad_TrackOccupancy=ibooker.book2D("IsoPfChHad_Track_profile","Isolated PFChHadron Tracker_occupancy", 108, -2.7, 2.7, 160, -M_PI,M_PI);
    map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+"IsoPfChHad_Track_profile"        ,mProfileIsoPFChHad_TrackOccupancy));
    mProfileIsoPFChHad_TrackPt=ibooker.book2D("IsoPfChHad_TrackPt","Isolated PFChHadron TrackPt", 108, -2.7, 2.7, 160, -M_PI,M_PI);
    map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+"IsoPfChHad_TrackPt"        ,mProfileIsoPFChHad_TrackPt));
    
    
    mProfileIsoPFChHad_EcalOccupancyCentral  = ibooker.book2D("IsoPfChHad_ECAL_profile_central","IsolatedPFChHa ECAL occupancy (Barrel)", 180, -1.479, 1.479, 125, -M_PI,M_PI);
    map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+"IsoPfChHad_ECAL_profile_central"        ,mProfileIsoPFChHad_EcalOccupancyCentral));
    mProfileIsoPFChHad_EMPtCentral=ibooker.book2D("IsoPfChHad_EMPt_central","Isolated PFChHadron HadPt_central", 180, -1.479, 1.479, 360, -M_PI,M_PI);
    map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+"IsoPfChHad_EMPt_central"        ,mProfileIsoPFChHad_EMPtCentral));
    
    mProfileIsoPFChHad_EcalOccupancyEndcap  = ibooker.book2D("IsoPfChHad_ECAL_profile_endcap","IsolatedPFChHa ECAL occupancy (Endcap)", 110, -2.75, 2.75, 125, -M_PI,M_PI);
    map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+"IsoPfChHad_ECAL_profile_endcap"        ,mProfileIsoPFChHad_EcalOccupancyEndcap));
    mProfileIsoPFChHad_EMPtEndcap=ibooker.book2D("IsoPfChHad_EMPt_endcap","Isolated PFChHadron EMPt_endcap", 110, -2.75, 2.75, 125, -M_PI,M_PI);
    map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+"IsoPfChHad_EMPt_endcap"        ,mProfileIsoPFChHad_EMPtEndcap));
    
    const int nbins_eta=16;
    
    double eta_limits[nbins_eta]={-2.650,-2.500,-2.322,-2.172,-2.043,-1.930,-1.830,-1.740,1.740,1.830,1.930,2.043,2.172,2.3122,2.500,2.650};
    
    TH2F* hist_temp_HCAL =new TH2F("IsoPfChHad_HCAL_profile_endcap","IsolatedPFChHa HCAL occupancy (outer endcap)",nbins_eta-1,eta_limits, 36, -M_PI,M_PI);
    TH2F* hist_tempPt_HCAL=(TH2F*)hist_temp_HCAL->Clone("Isolated PFCHHadron HadPt (outer endcap)");
    
    mProfileIsoPFChHad_HcalOccupancyCentral  = ibooker.book2D("IsoPfChHad_HCAL_profile_central","IsolatedPFChHa HCAL occupancy (Central Part)", 40, -1.740, 1.740, 72, -M_PI,M_PI);
    map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+"IsoPfChHad_HCAL_profile_central"        ,mProfileIsoPFChHad_HcalOccupancyCentral));
    mProfileIsoPFChHad_HadPtCentral=ibooker.book2D("IsoPfChHad_HadPt_central","Isolated PFChHadron HadPt_central", 40, -1.740, 1.740, 72, -M_PI,M_PI);
    map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+"IsoPfChHad_HadPt_central"        ,mProfileIsoPFChHad_HadPtCentral));
    
    mProfileIsoPFChHad_HcalOccupancyEndcap  = ibooker.book2D("IsoPfChHad_HCAL_profile_endcap",hist_temp_HCAL);
    map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+"IsoPfChHad_HCAL_profile_endcap"        ,mProfileIsoPFChHad_HcalOccupancyEndcap));
    mProfileIsoPFChHad_HadPtEndcap=ibooker.book2D("IsoPfChHad_HadPt_endcap",hist_tempPt_HCAL);
    map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+"IsoPfChHad_HadPt_endcap"        ,mProfileIsoPFChHad_HadPtEndcap));
  }else{
    if(!etaMinPFCand_.empty()){
      etaMinPFCand_.clear();
      etaMaxPFCand_.clear();
      typePFCand_.clear();
      countsPFCand_.clear();
      multiplicityPFCand_.clear();
      occupancyPFCand_.clear();
      occupancyPFCand_puppiNolepWeight_.clear();
      ptPFCand_.clear();
      ptPFCand_puppiNolepWeight_.clear();
      occupancyPFCand_name_.clear();
      ptPFCand_name_.clear();
      occupancyPFCand_name_puppiNolepWeight_.clear();
      ptPFCand_name_puppiNolepWeight_.clear();
      multiplicityPFCand_name_.clear();
    }
    for (std::vector<edm::ParameterSet>::const_iterator v = diagnosticsParameters_.begin(); v!=diagnosticsParameters_.end(); v++) {
      int etaNBinsPFCand = v->getParameter<int>("etaNBins");
      double etaMinPFCand = v->getParameter<double>("etaMin");
      double etaMaxPFCand = v->getParameter<double>("etaMax");
      int phiNBinsPFCand = v->getParameter<int>("phiNBins");
      double phiMinPFCand = v->getParameter<double>("phiMin");
      double phiMaxPFCand = v->getParameter<double>("phiMax");
      int nMinPFCand = v->getParameter<int>("nMin");
      int nMaxPFCand = v->getParameter<int>("nMax");
      int nbinsPFCand = v->getParameter<double>("nbins");
      
      // etaNBins_.push_back(etaNBins);
      etaMinPFCand_.push_back(etaMinPFCand);
      etaMaxPFCand_.push_back(etaMaxPFCand);
      typePFCand_.push_back(v->getParameter<int>("type"));
      countsPFCand_.push_back(0);
      
      multiplicityPFCand_.push_back(ibooker.book1D(std::string(v->getParameter<std::string>("name")).append("_multiplicity_").c_str(),std::string(v->getParameter<std::string>("name"))+"multiplicity", nbinsPFCand, nMinPFCand, nMaxPFCand));
      multiplicityPFCand_name_.push_back(std::string(v->getParameter<std::string>("name")).append("_multiplicity_").c_str());
      map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+ multiplicityPFCand_name_[multiplicityPFCand_name_.size()-1], multiplicityPFCand_[multiplicityPFCand_.size()-1]));
      //push back names first, we need to create histograms with the name and fill it for endcap plots later
      occupancyPFCand_name_.push_back(std::string(v->getParameter<std::string>("name")).append("_occupancy_puppiWeight_").c_str());
      ptPFCand_name_.push_back(std::string(v->getParameter<std::string>("name")).append("_pt_puppiWeight_").c_str());
      //push back names first, we need to create histograms with the name and fill it for endcap plots later
      occupancyPFCand_name_puppiNolepWeight_.push_back(std::string(v->getParameter<std::string>("name")).append("_occupancy_puppiNolepWeight_").c_str());	  
      ptPFCand_name_puppiNolepWeight_.push_back(std::string(v->getParameter<std::string>("name")).append("_pt_puppiNolepWeight_").c_str());
      //special booking for endcap plots, merge plots for eminus and eplus into one plot, using variable binning
      //barrel plots have eta-boundaries on minus and plus side
      //parameters start on minus side
      if(etaMinPFCand*etaMaxPFCand<0){//barrel plots, plot only in barrel region
	occupancyPFCand_.push_back(ibooker.book2D(std::string(v->getParameter<std::string>("name")).append("_occupancy_puppiWeight_").c_str(),std::string(v->getParameter<std::string>("name"))+"occupancy_puppiWeight_", etaNBinsPFCand, etaMinPFCand, etaMaxPFCand, phiNBinsPFCand, phiMinPFCand, phiMaxPFCand));
	ptPFCand_.push_back(ibooker.book2D(std::string(v->getParameter<std::string>("name")).append("_pt_puppiWeight_").c_str(),std::string(v->getParameter<std::string>("name"))+"pt_puppiWeight_", etaNBinsPFCand, etaMinPFCand, etaMaxPFCand, phiNBinsPFCand, phiMinPFCand, phiMaxPFCand));
	occupancyPFCand_puppiNolepWeight_.push_back(ibooker.book2D(std::string(v->getParameter<std::string>("name")).append("_occupancy_puppiNolepWeight_").c_str(),std::string(v->getParameter<std::string>("name"))+"occupancy_puppiNolepWeight", etaNBinsPFCand, etaMinPFCand, etaMaxPFCand, phiNBinsPFCand, phiMinPFCand, phiMaxPFCand));
	ptPFCand_puppiNolepWeight_.push_back(ibooker.book2D(std::string(v->getParameter<std::string>("name")).append("_pt_puppiNolepWeight_").c_str(),std::string(v->getParameter<std::string>("name"))+"pt_puppiNolepWeight", etaNBinsPFCand, etaMinPFCand, etaMaxPFCand, phiNBinsPFCand, phiMinPFCand, phiMaxPFCand));
      }else{//endcap or forward plots, 
	const int nbins_eta_endcap=2*(etaNBinsPFCand+1);
	double eta_limits_endcap[nbins_eta_endcap];
	for(int i=0;i<nbins_eta_endcap;i++){
	  if(i<(etaNBinsPFCand+1)){
	    eta_limits_endcap[i]=etaMinPFCand+i*(etaMaxPFCand-etaMinPFCand)/(double)etaNBinsPFCand;
	  }else{
	    eta_limits_endcap[i]= -etaMaxPFCand +(i- (etaNBinsPFCand+1) )*(etaMaxPFCand-etaMinPFCand)/(double)etaNBinsPFCand;
	  }
	}
	TH2F* hist_temp_occup = new TH2F((occupancyPFCand_name_[occupancyPFCand_name_.size()-1]).c_str(),"occupancy_puppiWeight",nbins_eta_endcap-1, eta_limits_endcap, phiNBinsPFCand, phiMinPFCand, phiMaxPFCand);
	occupancyPFCand_.push_back(ibooker.book2D(occupancyPFCand_name_[occupancyPFCand_name_.size()-1],hist_temp_occup));
	TH2F* hist_temp_pt = new TH2F((ptPFCand_name_[ptPFCand_name_.size()-1]).c_str(),"pt_puppiWeight",nbins_eta_endcap-1, eta_limits_endcap, phiNBinsPFCand, phiMinPFCand, phiMaxPFCand);
	ptPFCand_.push_back(ibooker.book2D(ptPFCand_name_[ptPFCand_name_.size()-1], hist_temp_pt));
	TH2F* hist_temp_occup_puppiNolepWeight = new TH2F((occupancyPFCand_name_puppiNolepWeight_[occupancyPFCand_name_puppiNolepWeight_.size()-1]).c_str(),"occupancy_puppiNolepWeight",nbins_eta_endcap-1, eta_limits_endcap, phiNBinsPFCand, phiMinPFCand, phiMaxPFCand);
	occupancyPFCand_puppiNolepWeight_.push_back(ibooker.book2D(occupancyPFCand_name_puppiNolepWeight_[occupancyPFCand_name_puppiNolepWeight_.size()-1],hist_temp_occup_puppiNolepWeight));
	TH2F* hist_temp_pt_puppiNolepWeight = new TH2F((ptPFCand_name_puppiNolepWeight_[ptPFCand_name_puppiNolepWeight_.size()-1]).c_str(),"pt_puppiNolepWeight",nbins_eta_endcap-1, eta_limits_endcap, phiNBinsPFCand, phiMinPFCand, phiMaxPFCand);
	ptPFCand_puppiNolepWeight_.push_back(ibooker.book2D(ptPFCand_name_puppiNolepWeight_[ptPFCand_name_puppiNolepWeight_.size()-1], hist_temp_pt_puppiNolepWeight));
      } 
      map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+ occupancyPFCand_name_puppiNolepWeight_[occupancyPFCand_name_puppiNolepWeight_.size()-1], occupancyPFCand_puppiNolepWeight_[occupancyPFCand_puppiNolepWeight_.size()-1]));
      map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+ ptPFCand_name_puppiNolepWeight_[ptPFCand_name_puppiNolepWeight_.size()-1], ptPFCand_puppiNolepWeight_[ptPFCand_puppiNolepWeight_.size()-1]));
      map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+ occupancyPFCand_name_[occupancyPFCand_name_.size()-1], occupancyPFCand_[occupancyPFCand_.size()-1]));
      map_of_MEs.insert(std::pair<std::string,MonitorElement*>(DirName+"/"+ ptPFCand_name_[ptPFCand_name_.size()-1], ptPFCand_[ptPFCand_.size()-1]));
    }
  }   
}	

// ***********************************************************
void PFCandidateAnalyzer::dqmBeginRun(const edm::Run& iRun, const edm::EventSetup& iSetup)
{

  miniaodfilterindex=-1;

  if(isMiniAOD_){
    bool changed_filter=true;
    if (FilterhltConfig_.init(iRun,iSetup,METFilterMiniAODLabel_.process(),changed_filter)){
      miniaodfilterdec=0;
      for(unsigned int i=0;i<FilterhltConfig_.size();i++){
	std::string search=FilterhltConfig_.triggerName(i).substr(5);//actual label of filter, the first 5 items are Flag_, so stripped off
	std::string search2=HBHENoiseStringMiniAOD;//all filters end with DQM, which is not in the flag --> ONLY not for HBHEFilters
	std::size_t found=search2.find(search);
	if(found!=std::string::npos){
	  miniaodfilterindex=i;
	}
      }
    }else if(FilterhltConfig_.init(iRun,iSetup,METFilterMiniAODLabel2_.process(),changed_filter)){
      miniaodfilterdec=1;
      for(unsigned int i=0;i<FilterhltConfig_.size();i++){
	std::string search=FilterhltConfig_.triggerName(i).substr(5);//actual label of filter, the first 5 items are Flag_, so stripped off
	std::string search2=HBHENoiseStringMiniAOD;//all filters end with DQM, which is not in the flag --> ONLY not for HBHEFilters
	std::size_t found=search2.find(search);
	if(found!=std::string::npos){
	  miniaodfilterindex=i;
	}
      }
    }else{
      std::cout<<"nothing found with both RECO and reRECO label"<<std::endl;
    }
  }
}

// ***********************************************************
void PFCandidateAnalyzer::endRun(const edm::Run& iRun, const edm::EventSetup& iSetup)
{
}

// ***********************************************************
void PFCandidateAnalyzer::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {


  //Vertex information
  Handle<VertexCollection> vertexHandle;
  iEvent.getByToken(vertexToken_, vertexHandle);

  if (!vertexHandle.isValid()) {
    LogDebug("") << "CaloMETAnalyzer: Could not find vertex collection" << std::endl;
    if (verbose_) std::cout << "CaloMETAnalyzer: Could not find vertex collection" << std::endl;
  }
  numPV_ = 0;
  if ( vertexHandle.isValid() ){
    VertexCollection vertexCollection = *(vertexHandle.product());
    numPV_  = vertexCollection.size();
  }
  bool bPrimaryVertex = (bypassAllPVChecks_ || (numPV_>0));

  int myLuminosityBlock;
  myLuminosityBlock = iEvent.luminosityBlock();

  if (myLuminosityBlock<LSBegin_) return;
  if (myLuminosityBlock>LSEnd_ && LSEnd_>0) return;

  if (verbose_) std::cout << "METAnalyzer analyze" << std::endl;

  std::string DirName = "JetMET/PFCandidates/"+mInputCollection_.label();


  bool hbhenoifilterdecision=true;
  if(!isMiniAOD_){//not checked for MiniAOD -> for miniaod decision filled as "triggerResults" bool
    edm::Handle<bool> HBHENoiseFilterResultHandle;
    iEvent.getByToken(hbheNoiseFilterResultToken_, HBHENoiseFilterResultHandle);
    if (!HBHENoiseFilterResultHandle.isValid()) {
      LogDebug("") << "METAnalyzer: Could not find HBHENoiseFilterResult" << std::endl;
      if (verbose_) std::cout << "METAnalyzer: Could not find HBHENoiseFilterResult" << std::endl;
    }
    hbhenoifilterdecision= *HBHENoiseFilterResultHandle;
  }else{//need to check if we go for version 1 or version 2
    edm::Handle<edm::TriggerResults> metFilterResults;
    iEvent.getByToken(METFilterMiniAODToken_, metFilterResults);
    if(metFilterResults.isValid()){
      if(miniaodfilterindex!=-1){
	hbhenoifilterdecision = metFilterResults->accept(miniaodfilterindex);
      }
    }else{
      iEvent.getByToken(METFilterMiniAODToken2_, metFilterResults);
      if(metFilterResults.isValid()){
	if(miniaodfilterindex!=-1){
	  hbhenoifilterdecision = metFilterResults->accept(miniaodfilterindex);
	}
      }
    }
  }

  //DCS Filter
  bool bDCSFilter = (bypassAllDCSChecks_ || DCSFilter_->filter(iEvent, iSetup));

  for (unsigned int i=0;i<countsPFCand_.size();i++) {
    countsPFCand_[i]=0;
  }

  if(bDCSFilter && hbhenoifilterdecision && bPrimaryVertex){
    if(isMiniAOD_){
      edm::Handle<std::vector<pat::PackedCandidate> > packedParticleFlow;
      iEvent.getByToken(pflowPackedToken_, packedParticleFlow);
      //11, 13, 22 for el/mu/gamma, 211 chargedHadron, 130 neutralHadrons, 1 and 2 hadHF and EGammaHF
      for (unsigned int i = 0; i < packedParticleFlow->size(); i++) {
	const pat::PackedCandidate& c = packedParticleFlow->at(i);
	for (unsigned int j=0; j<typePFCand_.size(); j++) {
	  if (fabs(c.pdgId())==typePFCand_[j]) {
	    //second check for endcap, if inside barrel Max and Min symmetric around 0
	    if ( ((c.eta()>etaMinPFCand_[j]) && (c.eta()<etaMaxPFCand_[j])) || ((c.eta()> (-etaMaxPFCand_[j])) && (c.eta()< (-etaMinPFCand_[j]))) ){
	      countsPFCand_[j]+=1;
	      ptPFCand_[j]   = map_of_MEs[DirName + "/"+ptPFCand_name_[j]];
	      if ( ptPFCand_[j]       && ptPFCand_[j]->getRootObject()) ptPFCand_[j]->Fill(c.eta(), c.phi(), c.pt()*c.puppiWeight());
	      occupancyPFCand_[j]   = map_of_MEs[DirName + "/"+occupancyPFCand_name_[j]];
	      if ( occupancyPFCand_[j]       && occupancyPFCand_[j]->getRootObject()) occupancyPFCand_[j]->Fill(c.eta(), c.phi(),c.puppiWeight());
	      ptPFCand_puppiNolepWeight_[j]   = map_of_MEs[DirName + "/"+ptPFCand_name_puppiNolepWeight_[j]];
	      if ( ptPFCand_puppiNolepWeight_[j]       && ptPFCand_puppiNolepWeight_[j]->getRootObject()) ptPFCand_puppiNolepWeight_[j]->Fill(c.eta(), c.phi(), c.pt()*c.puppiWeightNoLep());
	      occupancyPFCand_puppiNolepWeight_[j]   = map_of_MEs[DirName + "/"+occupancyPFCand_name_puppiNolepWeight_[j]];
	      if ( occupancyPFCand_puppiNolepWeight_[j]       && occupancyPFCand_puppiNolepWeight_[j]->getRootObject()) occupancyPFCand_puppiNolepWeight_[j]->Fill(c.eta(), c.phi(),c.puppiWeightNoLep());
	    }
	  }
	}
      }
    }else{
      edm::Handle<std::vector<reco::PFCandidate> > particleFlow;
      iEvent.getByToken(pflowToken_, particleFlow);
      //std::cout<<"do i get here "<<particleFlow->size()<<"/"<<typePFCand_.size()<<std::endl;
      for (unsigned int i = 0; i < particleFlow->size(); i++) {
	const reco::PFCandidate& c = particleFlow->at(i);
	for (unsigned int j=0; j<typePFCand_.size(); j++) {
	  if (c.particleId()==typePFCand_[j]) {
	    //second check for endcap, if inside barrel Max and Min symmetric around 0
	    if ( ((c.eta()>etaMinPFCand_[j]) && (c.eta()<etaMaxPFCand_[j])) || ((c.eta()> (-etaMaxPFCand_[j])) && (c.eta()< (-etaMinPFCand_[j]))) ){
	      countsPFCand_[j]+=1;
	      ptPFCand_[j]   = map_of_MEs[DirName + "/"+ptPFCand_name_[j]];
	      if ( ptPFCand_[j]       && ptPFCand_[j]->getRootObject()) ptPFCand_[j]->Fill(c.eta(), c.phi(), c.pt());
	      occupancyPFCand_[j]   = map_of_MEs[DirName + "/"+occupancyPFCand_name_[j]];
	      if ( occupancyPFCand_[j]       && occupancyPFCand_[j]->getRootObject()) occupancyPFCand_[j]->Fill(c.eta(), c.phi());
	    }
	    //fill quantities for isolated charged hadron quantities
	    //only for charged hadrons
	    if ( c.particleId() == 1 &&  c.pt() > ptMinCand_ ){
	      // At least 1 GeV in HCAL
	      double ecalRaw = c.rawEcalEnergy();
	      double hcalRaw = c.rawHcalEnergy();
	      if ( (ecalRaw + hcalRaw) > hcalMin_ ){
		const PFCandidate::ElementsInBlocks& theElements = c.elementsInBlocks();
		if( theElements.empty() ) continue;
		unsigned int iTrack=-999;
		std::vector<unsigned int> iECAL;// =999;
		std::vector<unsigned int> iHCAL;// =999;
		const reco::PFBlockRef blockRef = theElements[0].first;
		const edm::OwnVector<reco::PFBlockElement>& elements = blockRef->elements();
		// Check that there is only one track in the block.
		unsigned int nTracks = 0;
		for(unsigned int iEle=0; iEle<elements.size(); iEle++) {	         
		  // Find the tracks in the block
		  PFBlockElement::Type type = elements[iEle].type();
		  switch( type ) {
		  case PFBlockElement::TRACK:
		    iTrack = iEle;
		    nTracks++;
		    break;
		  case PFBlockElement::ECAL:
		    iECAL.push_back( iEle );
		    break;
		  case PFBlockElement::HCAL:
		    iHCAL.push_back( iEle );
		    break;
		  default:
		    continue;
		  } 
		}
		if ( nTracks == 1 ){
		  // Characteristics of the track
		  const reco::PFBlockElementTrack& et = dynamic_cast<const reco::PFBlockElementTrack &>( elements[iTrack] );
		  mProfileIsoPFChHad_TrackOccupancy=map_of_MEs[DirName+"/"+"IsoPfChHad_Track_profile"];
		  if (mProfileIsoPFChHad_TrackOccupancy  && mProfileIsoPFChHad_TrackOccupancy->getRootObject()) mProfileIsoPFChHad_TrackOccupancy->Fill(et.trackRef()->eta(),et.trackRef()->phi());
		  mProfileIsoPFChHad_TrackPt=map_of_MEs[DirName+"/"+"IsoPfChHad_TrackPt"];
		  if (mProfileIsoPFChHad_TrackPt  && mProfileIsoPFChHad_TrackPt->getRootObject()) mProfileIsoPFChHad_TrackPt->Fill(et.trackRef()->eta(),et.trackRef()->phi(),et.trackRef()->pt());
		  //ECAL element
		  for(unsigned int ii=0;ii<iECAL.size();ii++) {
		    const reco::PFBlockElementCluster& eecal = dynamic_cast<const reco::PFBlockElementCluster &>( elements[ iECAL[ii] ] );
		    if(fabs(eecal.clusterRef()->eta())<1.479){
		      mProfileIsoPFChHad_EcalOccupancyCentral=map_of_MEs[DirName+"/"+"IsoPfChHad_ECAL_profile_central"];
		      if (mProfileIsoPFChHad_EcalOccupancyCentral  && mProfileIsoPFChHad_EcalOccupancyCentral->getRootObject()) mProfileIsoPFChHad_EcalOccupancyCentral->Fill(eecal.clusterRef()->eta(),eecal.clusterRef()->phi());
		      mProfileIsoPFChHad_EMPtCentral=map_of_MEs[DirName+"/"+"IsoPfChHad_EMPt_central"];
		      if (mProfileIsoPFChHad_EMPtCentral  && mProfileIsoPFChHad_EMPtCentral->getRootObject()) mProfileIsoPFChHad_EMPtCentral->Fill(eecal.clusterRef()->eta(),eecal.clusterRef()->phi(),eecal.clusterRef()->pt());
		    }else{
		      mProfileIsoPFChHad_EcalOccupancyEndcap=map_of_MEs[DirName+"/"+"IsoPfChHad_ECAL_profile_endcap"];
		      if (mProfileIsoPFChHad_EcalOccupancyEndcap  && mProfileIsoPFChHad_EcalOccupancyEndcap->getRootObject()) mProfileIsoPFChHad_EcalOccupancyEndcap->Fill(eecal.clusterRef()->eta(),eecal.clusterRef()->phi());
		      mProfileIsoPFChHad_EMPtEndcap=map_of_MEs[DirName+"/"+"IsoPfChHad_EMPt_endcap"];
		      if (mProfileIsoPFChHad_EMPtEndcap  && mProfileIsoPFChHad_EMPtEndcap->getRootObject()) mProfileIsoPFChHad_EMPtEndcap->Fill(eecal.clusterRef()->eta(),eecal.clusterRef()->phi(),eecal.clusterRef()->pt());
		    }
		  }	
		  //HCAL element
		  for(unsigned int ii=0;ii<iHCAL.size();ii++) {
		    const reco::PFBlockElementCluster& ehcal = dynamic_cast<const reco::PFBlockElementCluster &>( elements[ iHCAL[ii] ] );
		    if(fabs(ehcal.clusterRef()->eta())<1.740){
		      mProfileIsoPFChHad_HcalOccupancyCentral=map_of_MEs[DirName+"/"+"IsoPfChHad_HCAL_profile_central"];
		      if (mProfileIsoPFChHad_HcalOccupancyCentral  && mProfileIsoPFChHad_HcalOccupancyCentral->getRootObject()) mProfileIsoPFChHad_HcalOccupancyCentral->Fill(ehcal.clusterRef()->eta(),ehcal.clusterRef()->phi());
		      mProfileIsoPFChHad_HadPtCentral=map_of_MEs[DirName+"/"+"IsoPfChHad_HadPt_central"];
		      if (mProfileIsoPFChHad_HadPtCentral  && mProfileIsoPFChHad_HadPtCentral->getRootObject()) mProfileIsoPFChHad_HadPtCentral->Fill(ehcal.clusterRef()->eta(),ehcal.clusterRef()->phi(),ehcal.clusterRef()->pt());
		    }else{
		      mProfileIsoPFChHad_HcalOccupancyEndcap=map_of_MEs[DirName+"/"+"IsoPfChHad_HCAL_profile_endcap"];
		      if (mProfileIsoPFChHad_HcalOccupancyEndcap  && mProfileIsoPFChHad_HcalOccupancyEndcap->getRootObject()) mProfileIsoPFChHad_HcalOccupancyEndcap->Fill(ehcal.clusterRef()->eta(),ehcal.clusterRef()->phi());
		      mProfileIsoPFChHad_HadPtEndcap=map_of_MEs[DirName+"/"+"IsoPfChHad_HadPt_endcap"];
		      if (mProfileIsoPFChHad_HadPtEndcap  && mProfileIsoPFChHad_HadPtEndcap->getRootObject()) mProfileIsoPFChHad_HadPtEndcap->Fill(ehcal.clusterRef()->eta(),ehcal.clusterRef()->phi(),ehcal.clusterRef()->pt());
		    }	
		  }
		}
	      }
	    }
	  }	
	}
      }
    }//candidate loop for both miniaod and reco
    for (unsigned int j=0; j<countsPFCand_.size(); j++) {
      multiplicityPFCand_[j]   = map_of_MEs[DirName + "/"+multiplicityPFCand_name_[j]];
      if(multiplicityPFCand_[j] && multiplicityPFCand_[j]->getRootObject()){	multiplicityPFCand_[j]->Fill(countsPFCand_[j]);
      }
    }
  }
}
