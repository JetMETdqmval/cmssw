import os
import FWCore.ParameterSet.Config as cms

process = cms.Process("JetMETDQMOffline")
process.load("FWCore.MessageLogger.MessageLogger_cfi")
process.MessageLogger.cerr.FwkReport.reportEvery = 1000

from Configuration.StandardSequences.GeometryRecoDB_cff import *
process.load('Configuration.StandardSequences.GeometryRecoDB_cff')
process.load('Configuration.StandardSequences.MagneticField_cff')
process.load('Configuration.StandardSequences.Reconstruction_cff')
process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_cff')
process.load('Configuration.StandardSequences.Services_cff')

#for data in 720pre7
process.GlobalTag.globaltag ='80X_dataRun2_v4'

# check # of bins
process.load("DQMServices.Components.DQMStoreStats_cfi")

readFiles = cms.untracked.vstring()
secFiles = cms.untracked.vstring() 
process.source = cms.Source ("PoolSource",fileNames = readFiles, secondaryFileNames = secFiles)
readFiles.extend( [
       '/store/relval/CMSSW_8_0_0_pre6/JetHT/MINIAOD/80X_dataRun2_v4_multiCoreResub_RelVal_jetHT2015D-v1/10000/3CE7A3C8-E8D6-E511-A91C-0CC47A78A436.root',
       '/store/relval/CMSSW_8_0_0_pre6/JetHT/MINIAOD/80X_dataRun2_v4_multiCoreResub_RelVal_jetHT2015D-v1/10000/545783CA-E8D6-E511-A610-0025905B85CA.root',
       '/store/relval/CMSSW_8_0_0_pre6/JetHT/MINIAOD/80X_dataRun2_v4_multiCoreResub_RelVal_jetHT2015D-v1/10000/5C7684C5-E8D6-E511-B9F7-0CC47A7452D0.root',
       '/store/relval/CMSSW_8_0_0_pre6/JetHT/MINIAOD/80X_dataRun2_v4_multiCoreResub_RelVal_jetHT2015D-v1/10000/769043CD-E8D6-E511-8CC7-0CC47A78A3EE.root'
#'/store/relval/CMSSW_8_0_0_pre6/RelValQCD_FlatPt_15_3000HS_13/GEN-SIM-RECO/80X_mcRun2_asymptotic_v4-v1/10000/2A7077F8-DDD0-E511-81E1-0025905B860E.root',
#'/store/relval/CMSSW_8_0_0_pre6/RelValQCD_FlatPt_15_3000HS_13/GEN-SIM-RECO/80X_mcRun2_asymptotic_v4-v1/10000/2C4C2526-E3D0-E511-A45A-002618FDA248.root',
#'/store/relval/CMSSW_8_0_0_pre6/RelValQCD_FlatPt_15_3000HS_13/GEN-SIM-RECO/80X_mcRun2_asymptotic_v4-v1/10000/4C0F14C4-DCD0-E511-AD33-0CC47A4D76A2.root',
#'/store/relval/CMSSW_8_0_0_pre6/RelValQCD_FlatPt_15_3000HS_13/GEN-SIM-RECO/80X_mcRun2_asymptotic_v4-v1/10000/5ABF258D-DFD0-E511-9F0D-0CC47A4D766C.root',
#'/store/relval/CMSSW_8_0_0_pre6/RelValQCD_FlatPt_15_3000HS_13/GEN-SIM-RECO/80X_mcRun2_asymptotic_v4-v1/10000/6AE93260-DED0-E511-B224-0CC47A4C8ECA.root',
#'/store/relval/CMSSW_8_0_0_pre6/RelValQCD_FlatPt_15_3000HS_13/GEN-SIM-RECO/80X_mcRun2_asymptotic_v4-v1/10000/76394723-DDD0-E511-960D-0025905A60EE.root',
#'/store/relval/CMSSW_8_0_0_pre6/RelValQCD_FlatPt_15_3000HS_13/GEN-SIM-RECO/80X_mcRun2_asymptotic_v4-v1/10000/90D569F9-DDD0-E511-9CAA-0CC47A4C8E98.root',
#'/store/relval/CMSSW_8_0_0_pre6/RelValQCD_FlatPt_15_3000HS_13/GEN-SIM-RECO/80X_mcRun2_asymptotic_v4-v1/10000/9E532FC7-E1D0-E511-B6B0-0CC47A78A472.root',
#'/store/relval/CMSSW_8_0_0_pre6/RelValQCD_FlatPt_15_3000HS_13/GEN-SIM-RECO/80X_mcRun2_asymptotic_v4-v1/10000/9EC84D1E-E3D0-E511-97D3-0026189438EF.root',
#'/store/relval/CMSSW_8_0_0_pre6/RelValQCD_FlatPt_15_3000HS_13/GEN-SIM-RECO/80X_mcRun2_asymptotic_v4-v1/10000/B8E9A1C7-DCD0-E511-BA3D-0025905B8612.root',
#'/store/relval/CMSSW_8_0_0_pre6/RelValQCD_FlatPt_15_3000HS_13/GEN-SIM-RECO/80X_mcRun2_asymptotic_v4-v1/10000/C4C8F842-DED0-E511-B8D5-0025905A48E4.root',
#'/store/relval/CMSSW_8_0_0_pre6/RelValQCD_FlatPt_15_3000HS_13/GEN-SIM-RECO/80X_mcRun2_asymptotic_v4-v1/10000/CE9E4596-DBD0-E511-AE7E-0025905A60F8.root',
#'/store/relval/CMSSW_8_0_0_pre6/RelValQCD_FlatPt_15_3000HS_13/GEN-SIM-RECO/80X_mcRun2_asymptotic_v4-v1/10000/DCB8CE76-E0D0-E511-9DF5-0CC47A4D7600.root',
       ] );


secFiles.extend( [
               ] )

#
process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32( -1 )
)

process.load("DQMOffline.JetMET.dataCertificationJetMET_cff")

process.load('Configuration/StandardSequences/EDMtoMEAtJobEnd_cff')
process.dqmSaver.referenceHandling = cms.untracked.string('all')
#CMSSW expects same sequence name for different modes, comment out unneeded modes
#pp MC
#process.load("DQMOffline.JetMET.jetMETDQMOfflineSourceMC_cff")
#pp data
process.load("DQMOffline.JetMET.jetMETDQMOfflineSource_cff")
#cosmic data
#process.load("DQMOffline.JetMET.jetMETDQMOfflineSourceCosmic_cff")
#cosmic MC
#process.load("DQMOffline.JetMET.jetMETDQMOfflineSourceCosmicMC_cff")
#for HI data
#process.load("DQMOffline.JetMET.jetMETDQMOfflineSourceHI_cff")
#for HI MC
#process.load("DQMOffline.JetMET.jetMETDQMOfflineSourceHIMC_cff")

cmssw_version = os.environ.get('CMSSW_VERSION','CMSSW_X_Y_Z')
Workflow = '/JetMET/'+str(cmssw_version)+'/Harvesting'
process.dqmSaver.workflow = Workflow

process.dump = cms.EDAnalyzer("EventContentAnalyzer")

process.p = cms.Path(                    #process.dump*
                     process.jetMETDQMOfflineSourceMiniAOD*
                     #for cosmic data and MC
                     #process.jetMETDQMOfflineSourceCosmic*
                     #for Data and MC pp and HI
                     #process.jetMETDQMOfflineSource*
                     process.dataCertificationJetMETSequence*
                     process.dqmSaver
                     )
