import os
import FWCore.ParameterSet.Config as cms

process = cms.Process("METVALIDATION")


process.load("Configuration.StandardSequences.GeometryDB_cff")
process.load("Configuration.StandardSequences.Reconstruction_cff")
process.load("Configuration/StandardSequences/MagneticField_cff")
process.load('Configuration/StandardSequences/FrontierConditions_GlobalTag_cff')

process.GlobalTag.globaltag = 'PRE_LS172_V15::All'
process.load('Configuration/StandardSequences/FrontierConditions_GlobalTag_cff')

process.load("FWCore.MessageLogger.MessageLogger_cfi")
process.MessageLogger.cerr.FwkReport.reportEvery = 1000
#
#
# DQM
#
process.load("DQMServices.Core.DQM_cfg")


process.load("Validation.RecoMET.METRelValForDQM_cff")


readFiles = cms.untracked.vstring()
secFiles = cms.untracked.vstring() 
process.source = cms.Source ("PoolSource",fileNames = readFiles, secondaryFileNames = secFiles)
readFiles.extend( [
       #for RECO
       #'/store/relval/CMSSW_7_2_0_pre8/RelValTTbar_13/GEN-SIM-RECO/PU25ns_PRE_LS172_V15-v1/00000/42783A1F-1550-E411-B888-0025905B8582.root',
       #'/store/relval/CMSSW_7_2_0_pre8/RelValTTbar_13/GEN-SIM-RECO/PU25ns_PRE_LS172_V15-v1/00000/A233AB64-0C50-E411-A954-0025905A60A0.root',
       #'/store/relval/CMSSW_7_2_0_pre8/RelValTTbar_13/GEN-SIM-RECO/PU25ns_PRE_LS172_V15-v1/00000/C60B9C19-1550-E411-8517-002618FDA248.root',
       #'/store/relval/CMSSW_7_2_0_pre8/RelValTTbar_13/GEN-SIM-RECO/PU25ns_PRE_LS172_V15-v1/00000/DCA629A7-0C50-E411-9AC6-002618943849.root',
       #'/store/relval/CMSSW_7_2_0_pre8/RelValTTbar_13/GEN-SIM-RECO/PU25ns_PRE_LS172_V15-v1/00000/F20A822B-0B50-E411-99AC-0025905A60B4.root'
       #for MINIAODtests 
       '/store/relval/CMSSW_7_2_0_pre8/RelValTTbar_13/MINIAODSIM/PU25ns_PRE_LS172_V15-v1/00000/1A860BD4-2150-E411-BB97-0025905A60D0.root',
       '/store/relval/CMSSW_7_2_0_pre8/RelValTTbar_13/MINIAODSIM/PU25ns_PRE_LS172_V15-v1/00000/72286AD5-2150-E411-A2B3-0025905B8572.root' 
] );

process.load('Configuration/StandardSequences/EDMtoMEAtJobEnd_cff')
process.dqmSaver.referenceHandling = cms.untracked.string('all')
#
cmssw_version = os.environ.get('CMSSW_VERSION','CMSSW_X_Y_Z')
Workflow = '/JetMET/'+str(cmssw_version)+'/METValidation'
process.dqmSaver.workflow = Workflow
process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(-1) )


process.p = cms.Path(#for RECO
                     #process.METValidation
                     #for MiniAOD
                     process.METValidationMiniAOD
                     *process.dqmSaver
)


