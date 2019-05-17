#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Framework/interface/EDProducer.h"
#include "FWCore/Utilities/interface/StreamID.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/ESHandle.h"

#include "DataFormats/Candidate/interface/Candidate.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include "DataFormats/PatCandidates/interface/Muon.h"

#include <iostream>
#include <string>
#include <map>

#include "VtxUtils.hh"

using namespace std;

class MCTruthB2DstMuProducer : public edm::EDProducer {

public:

    explicit MCTruthB2DstMuProducer(const edm::ParameterSet &iConfig);
    void AddTLVToOut(TLorentzVector, string, map<string, float>*);

    ~MCTruthB2DstMuProducer() override {};

private:

    virtual void produce(edm::Event&, const edm::EventSetup&);

    // ----------member data ---------------------------

    edm::EDGetTokenT<vector<reco::GenParticle>> PrunedParticlesSrc_;
    edm::EDGetTokenT<vector<pat::Muon>> TrgMuonSrc_;

    double mass_B0 = 5.27961;
    double mass_mu = 0.1056583745;
    double mass_K = 0.493677;
    double mass_pi = 0.13957018;

    int verbose = 0;
};



MCTruthB2DstMuProducer::MCTruthB2DstMuProducer(const edm::ParameterSet &iConfig)
{
    verbose = iConfig.getParameter<int>( "verbose" );
    PrunedParticlesSrc_ = consumes<vector<reco::GenParticle>>(edm::InputTag("prunedGenParticles"));
    TrgMuonSrc_ = consumes<vector<pat::Muon>> ( iConfig.getParameter<edm::InputTag>( "trgMuons" ) );

    produces<map<string, float>>("outputNtuplizer");
}


void MCTruthB2DstMuProducer::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {
    if (verbose) {cout << "-----------  MC Truth ----------\n";}

    // Get prunedGenParticles
    edm::Handle<std::vector<reco::GenParticle>> PrunedGenParticlesHandle;
    iEvent.getByToken(PrunedParticlesSrc_, PrunedGenParticlesHandle);

    edm::Handle<vector<pat::Muon>> trgMuonsHandle;
    iEvent.getByToken(TrgMuonSrc_, trgMuonsHandle);
    auto trgMu = (*trgMuonsHandle)[0];


    // Output collection
    unique_ptr<map<string, float>> outputNtuplizer(new map<string, float>);

    map<string, TLorentzVector> p4;
    p4["B"] = TLorentzVector();
    p4["mu"] = TLorentzVector();
    p4["Dst"] = TLorentzVector();
    p4["pis"] = TLorentzVector();
    p4["D0"] = TLorentzVector();

    if(verbose) {
      uint nb = 0;
      for(auto p : *PrunedGenParticlesHandle) {
        if (p.pdgId() == 511 && p.numberOfDaughters()>1) {
          nb++;
          cout << "Found: " << p.pdgId() << " (-->" << flush;
          for(auto d : p.daughterRefVector()) {
            cout << " " << d->pdgId() << flush;
          }
          cout << ')' << endl;
        }
      }
      cout << nb << endl;
    }

    for(auto p : *PrunedGenParticlesHandle) {
      if (p.pdgId() == 511 && p.numberOfDaughters()>1) {
        p4["B"].SetPtEtaPhiM(p.pt(), p.eta(), p.phi(), p.mass());

        for(auto d : p.daughterRefVector()) {
          if(d->pdgId() == -13) {
            p4["mu"].SetPtEtaPhiM(d->pt(), d->eta(), d->phi(), d->mass());
          }
          if(d->pdgId() == -15) {
            for (auto dd : d->daughterRefVector()) {
              if (dd->pdgId() == -13) {
                p4["mu"].SetPtEtaPhiM(dd->pt(), dd->eta(), dd->phi(), dd->mass());
              }
            }
          }
          else if(d->pdgId() == -413) {
            p4["Dst"].SetPtEtaPhiM(d->pt(), d->eta(), d->phi(), d->mass());

            for(auto dd : d->daughterRefVector()) {
              if(dd->pdgId() == -211) {
                p4["pis"].SetPtEtaPhiM(dd->pt(), dd->eta(), dd->phi(), dd->mass());
              }
              else if(abs(dd->pdgId()) == 421) {
                p4["D0"].SetPtEtaPhiM(dd->pt(), dd->eta(), dd->phi(), dd->mass());
                // for(auto ddd : dd->daughterRefVector()) {
                //   if (ddd->pdgId() == -211 || ddd->pdgId() == 321)
                //   {
                //     string name = ddd->pdgId() == -211 ? "pi" : "K";
                //
                //   }
                // }
              }
            }
          }
        }

        double dPhi_mu = vtxu::dPhi(trgMu.phi(), p4["mu"].Phi());
        double dEta_mu = trgMu.eta() - p4["mu"].Eta();
        double dPt_rel_mu = (trgMu.pt() - p4["mu"].Pt())/trgMu.pt();
        if (fabs(dPhi_mu) < 0.01 && fabs(dEta_mu) < 0.01 && fabs(dPt_rel_mu) < 0.1 ) break;
      }
    }


    for(auto kv : p4) {
      AddTLVToOut(kv.second, "MC_"+kv.first, &(*outputNtuplizer));
    }

    if(verbose) {
      cout << "Distance between MC muon and trigger muon" << endl;
      cout << "dPhi: " << vtxu::dPhi(trgMu.phi(), p4["mu"].Phi()) << endl;
      cout << "dEta: " << trgMu.eta() - p4["mu"].Eta() << endl;
      cout << "dPt: " << (trgMu.pt() - p4["mu"].Pt())/trgMu.pt() << endl;
    }

    (*outputNtuplizer)["MC_M2_miss"] = (p4["B"] - p4["Dst"] - p4["mu"]).M2();
    (*outputNtuplizer)["MC_q2"] = (p4["B"] - p4["Dst"]).M2();

    TLorentzVector p4st_mu(p4["mu"]);
    p4st_mu.Boost(-1*p4["B"].BoostVector());
    (*outputNtuplizer)["MC_Est_mu"] = p4st_mu.E();


    iEvent.put(move(outputNtuplizer), "outputNtuplizer");
    return;
}


void MCTruthB2DstMuProducer::AddTLVToOut(TLorentzVector v, string n, map<string, float>* outv) {
  (*outv)[n+"_pt"] = v.Pt();
  (*outv)[n+"_eta"] = v.Eta();
  (*outv)[n+"_phi"] = v.Phi();
  (*outv)[n+"_P"] = v.P();
  return;
}


DEFINE_FWK_MODULE(MCTruthB2DstMuProducer);