#!/usr/bin/env python
import yaml
import os
import datetime

import argparse
#Example: python submitCMSSWCrabJobs.py B2DstMu -e D C
cfg = {
       # Ancillary
       # 'triggerMu': 'cmssw_cmsRD2018_triggerMuon.py',
       'TagAndProbeTrigger': 'cmssw_cmsRD2018_TagAndProbeTrigger.py',
       'B2JpsiKst': 'cmssw_cmsRD2018_Tag_Mu-Probe-B0_JpsiKst-mumuKpi.py',
       # 'B2JpsiK': toBeUpdated
       # Main
       'B2DstMu': 'cmssw_cmsRD2018_Tag_Bd_MuDst-PiPiK.py',
       'SSDstMu': 'cmssw_cmsRD2018_Tag_SS_MuDst-PiPiK.py'
       }

parser = argparse.ArgumentParser()
parser.add_argument ('tag', type=str, choices=cfg.keys(), help='Tag identifying the production')

parser.add_argument ('-e', '--eras', type=str, default=['A', 'B', 'C', 'D'], help='Eras to run on', nargs='+')
parser.add_argument ('-p', '--parts', type=int, default=[1, 2, 3, 4, 5, 6], help='Parts to run on', nargs='+')
parser.add_argument ('-t', '--time', type=int, default=6, help='Time expected for the jobs in hours (limits 3h - 45h).')
parser.add_argument ('--splitting', type=str, default='FileBased', choices=['Automatic', 'FileBased', 'LumiBased'])
parser.add_argument ('--ignoreLocality', type=str, default='False', choices=['True', 'False'])


parser.add_argument ('--wait', default=False, action='store_true')
args = parser.parse_args()

tag = args.tag

date = datetime.datetime.today()
date_str = '{}{:02}{:02}'.format(date.year%100, date.month, date.day)

prod_samples = yaml.load(open('../production/samples.yml'))

def dumpCrabConfig(k, dataset, settings):
    ds_list = dataset.split('/')
    fname = 'cfg' + dataset.replace('/', '_') + '.py'
    fout = open('tmp/' + fname, 'w')
    fout.write(
'''# To submit: crab submit -c CMSSWCrabConfig.py
from WMCore.Configuration import Configuration
config = Configuration()
config.section_('General')
config.General.transferOutputs = True
config.General.transferLogs = False
'''
    )
    dataset_tag = '{}_{}_RDntuplizer_{}_{}'.format(ds_list[1], ds_list[2], tag, date_str)
    fout.write("config.General.requestName = '{}'".format(dataset_tag))
    fout.write('\n')
    fout.write("config.section_('JobType')")
    fout.write('\n')
    fout.write("config.JobType.pluginName = 'Analysis'")
    fout.write('\n')
    fout.write("config.JobType.psetName = '{}/src/ntuplizer/BPH_RDntuplizer/config/{}'".format(os.environ['CMSSW_BASE'], cfg[tag]))
    fout.write('\n')
    fout.write("config.JobType.pyCfgParams = ['useLocalLumiList=0']")
    fout.write('\n')
    fout.write("config.JobType.outputFiles = ['{}_CAND.root']".format(tag))
    if not args.splitting == 'Automatic':
        fout.write('\n')
        fout.write("config.JobType.maxJobRuntimeMin = {}".format(60*args.time))
    fout.write('\n')
    fout.write("config.JobType.maxMemoryMB = 2000")
    fout.write('\n')
    fout.write("config.JobType.allowUndistributedCMSSW = True")
    fout.write('\n')
    fout.write("config.section_('Data')")
    fout.write('\n')
    fout.write("config.Data.inputDataset = '{}'".format(dataset))
    fout.write('\n')
    fout.write("config.Data.lumiMask = '/storage/af/user/ocerri/work/CMSSW_10_2_3/src/ntuplizer/BPH_RDntuplizer/production/Cert_314472-325175_13TeV_17SeptEarlyReReco2018ABC_PromptEraD_Collisions18_JSON.txt'")
    # fout.write("config.Data.lumiMask = '{}'".format(settings['common']['data']['lumimask']))
    fout.write('\n')
    fout.write("config.Data.publication = False")
    fout.write('\n')
    if args.splitting == 'Automatic':
        fout.write("config.Data.unitsPerJob = {}".format(60*args.time))
    elif args.splitting == 'FileBased':
        fout.write("config.Data.unitsPerJob = {}".format(settings['common']['data']['filesPerJob']))
    fout.write('\n')

    fout.write("config.Data.publishDBS = 'https://cmsweb.cern.ch/dbs/prod/phys03/DBSWriter/'")
    fout.write('\n')
    fout.write("config.Data.splitting = '{}'".format(args.splitting))
    fout.write('\n')
    fout.write("config.Data.inputDBS = 'https://cmsweb.cern.ch/dbs/prod/global/DBSReader/'")
    fout.write('\n')
    fout.write("config.Data.outputDatasetTag = '{}'".format(dataset_tag))
    fout.write('\n')
    fout.write("config.Data.ignoreLocality = {}".format(args.ignoreLocality)) # requires a whitelist
    fout.write('\n')
    fout.write("config.Data.allowNonValidInputDataset = True")
    fout.write('\n')
    fout.write("config.section_('Site')")
    fout.write('\n')
    fout.write("config.Site.storageSite = 'T2_US_Caltech'")
    if args.ignoreLocality == 'True':
        fout.write('\n')
        fout.write("config.Site.whitelist = ['T2_US_*', 'T2_IT_*', 'T2_CH_*', 'T2_DE_*']")
    fout.write('\n')
    fout.close()

    return fname, dataset_tag

if not os.path.isdir('tmp'):
    os.system('mkdir tmp')

for k, d in prod_samples['samples'].iteritems():
    if not k.startswith('data'):
        continue
    idx = k.find('Run2018')
    era = k[idx + len('Run2018')]
    if era in args.eras:
        for i in d['parts']:
            if not i in args.parts:
                continue
            dataset = d['dataset'].format(i)
            print '\n########## {} ##########\n'.format(dataset)

            fname, dset_tag = dumpCrabConfig(k, dataset, prod_samples)
            if os.path.isdir('tmp/crab_' + dset_tag):
                os.system('rm -rf tmp/crab_' + dset_tag)
            print '---> Config dump'
            os.system('cat tmp/' + fname)
            print ''
            cmd = 'cd tmp; source /cvmfs/cms.cern.ch/crab3/crab.sh;'
            cmd += ' crab submit -c ' + fname
            if args.wait:
                cmd += ' --wait'
            os.system(cmd)
            print '\n\n'
