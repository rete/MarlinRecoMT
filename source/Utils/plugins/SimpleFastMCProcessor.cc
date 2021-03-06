
// -- marlin headers
#include <marlin/Processor.h>
#include <marlin/PluginManager.h>

// -- MarlinRecoMT headers
#include <MarlinRecoMT/SimpleParticleFactory.h>
#include <MarlinRecoMT/SimpleTrackSmearer.h>
#include <MarlinRecoMT/SimpleClusterSmearer.h>
#include <MarlinRecoMT/FastMCParticleType.h>
#include <MarlinRecoMT/ErrorOfSigma.h>
#include <MarlinRecoMT/IRecoParticleFactory.h>

// -- lcio headers
#include <IMPL/LCCollectionVec.h>
#include <EVENT/ReconstructedParticle.h>
#include <UTIL/LCRelationNavigator.h>
#include <EVENT/LCIO.h>

namespace marlinreco_mt {

  /** A simple smearing "Monte Carlo" processor.
   *  It creates ReconstructedParticles from MCParticles according to
   *  the resolution that is specified for the particle type, one of:
   *  <ul>
   *  <li>photon</li>
   *  <li>charged: e+-,mu+-,pi+-,K+-,p+-,....</li>
   *  <li>neutral hadron: K0L, n, Lambda0,...</li>
   *  </ul>
   *  The resolutions for charged particles are given as delta(1/P) for a certain polar angle
   *  range (mapped to [0.,pi/2.]), e.g. <br>
   *  <b>ChargedResolution  &nbsp; .7e-5   &nbsp; 0.   &nbsp; 3.141593/2. </b><br>
   *  sets the resolution for all charged particles to 0.7*10^-5.<br>
   *  Specifying different resolutions for different polar angle ranges allows to mimic degrading
   *  detector performance in the very forward region. <br>
   *  The energy of the charged ReconstructedParticles is set from the "measured"
   *  momentum using the energy momentum relation for electrons and muons and assuming the pion mass
   *  for all other charged tracks.<br>
   *  The resolutions for neutral particles are given as A and B for a certain polar angle range,
   *  where dE/E = A "+" B / sqrt(E/GeV), e.g. <br>
   *  <b>NeutralHadronResolution   &nbsp; 0.03  &nbsp; .30  &nbsp;  0.  &nbsp;  3.141593/2. </b><br>
   *  sets the resolution for all neutral hadrons to 3% "+" 30% / sqrt( E /GeV ).
   *  The resolution for gammas is specified in <b>PhotonResolution</b><br>
   *  No ReconstructedParticles are created if there is no resolution defined at that polar angle, e.g.<br>
   *  <b>PhotonResolution  &nbsp; .7e-5   &nbsp; 0.083   &nbsp; 3.141593/2. </b><br>
   *  effectively limits the acceptance region for photons to theta > 83mrad.<br>
   *
   *  A collection of LCRelations, called "MCTruthMapping" holds the relation between the
   *  ReconstructedParticles and their proper MCParticles.
   *
   *
   *  <h4>Input - Prerequisites</h4>
   *  A collection of MCParticles (the MCPArticle collection).
   *
   *  <h4>Output</h4>
   *  <ul>
   *  <li><b>ReconstructedParticles</b>: the collection of reconstructed particles</li>
   *  <li><b>MCTruthMapping</b>:  holds LCRelations  that map the  ReconstructedParticles to their
   *       proper MCParticles </li>
   *  </ul>
   *
   * @param ChargedResolution    Resolution of charged particles in polar angle range:  d(1/P)  th_min  th_max
   * @param InputCollectionName  Name of the MCParticle input collection
   * @param MomentumCut          No reconstructed particles are produced for smaller momenta (in [GeV])
   * @param NeutralHadronResolution Resolution dE/E=A+B/sqrt(E/GeV) of neutral hadrons in polar angle range: A  B th_min  th_max
   * @param PhotonResolution   Resolution dE/E=A+B/sqrt(E/GeV) of photons in polar angle range: A  B th_min  th_max
   *
   * @param RecoParticleCollectionName    default is "ReconstructedParticles"
   * @param MCTruthMappingCollectionName  default is "MCTruthMapping"
   *
   *  @author F. Gaede, DESY
   *  @version $Id: SimpleFastMCProcessor.h,v 1.4 2007-07-04 12:13:06 gaede Exp $
   */

  class SimpleFastMCProcessor : public marlin::Processor {
  public:
    SimpleFastMCProcessor() ;
    SimpleFastMCProcessor(const SimpleFastMCProcessor&) = delete;
    SimpleFastMCProcessor& operator=(const SimpleFastMCProcessor&) = delete;
    void init() ;
    void processEvent( EVENT::LCEvent * evt ) ;

  private:
    marlin::InputCollectionProperty _inputCollectionName {this, EVENT::LCIO::MCPARTICLE, "InputCollectionName" ,
           "Name of the MCParticle input collection", "MCParticle" } ;

    marlin::OutputCollectionProperty _recoParticleCollectionName {this, EVENT::LCIO::RECONSTRUCTEDPARTICLE, "RecoParticleCollectionName" ,
           "Name of the ReconstructedParticles output collection", "ReconstructedParticles" } ;

    marlin::OutputCollectionProperty _mcTruthCollectionName {this, EVENT::LCIO::LCRELATION, "MCTruthMappingCollectionName" ,
           "Name of the MCTruthMapping output collection", "MCTruthMapping" } ;

    marlin::Property<float> _momentumCut {this, "MomentumCut" ,
           "No reconstructed particles are produced for smaller momenta (in [GeV])", 0.001 } ;

    marlin::Property<std::vector<float>> _initChargedRes {this, "ChargedResolution" ,
        "Resolution of charged particles in polar angle range:  d(1/P)  th_min  th_max", { 5e-5, 0.0, 3.141593/2.} } ;

    marlin::Property<std::vector<float>> _initPhotonRes {this, "PhotonResolution" ,
        "Resolution dE/E=A+B/sqrt(E/GeV) of photons in polar angle range: A  B th_min  th_max", {0.01, 0.1, 0., 3.141593/2.} } ;

    marlin::Property<std::vector<float>> _initNeutralHadronRes {this, "NeutralHadronResolution" ,
        "Resolution dE/E=A+B/sqrt(E/GeV) of neutral hadrons in polar angle range: A  B th_min  th_max", {0.04, 0.5, 0., 3.141593/2.} } ;
    
    ///< The particle factory
    IRecoParticleFactory    *_factory {nullptr} ;
  };

  //--------------------------------------------------------------------------
  //--------------------------------------------------------------------------

  SimpleFastMCProcessor::SimpleFastMCProcessor() :
    Processor("SimpleFastMCProcessor") {

    // modify processor description
    _description = "SimpleFastMCProcessor creates ReconstrcutedParticles from MCParticles "
      "according to the resolution given in the steering file." ;

    forceRuntimeOption( Processor::RuntimeOption::Clone, true ) ;
  }

  //--------------------------------------------------------------------------

  void SimpleFastMCProcessor::init() {
    // usually a good idea to
    printParameters() ;

    SimpleParticleFactory* simpleFactory  =  new SimpleParticleFactory() ;
    simpleFactory->registerIFourVectorSmearer(  new SimpleTrackSmearer( _initChargedRes ), CHARGED ) ;
    simpleFactory->registerIFourVectorSmearer(  new SimpleClusterSmearer( _initPhotonRes ), PHOTON ) ;
    simpleFactory->registerIFourVectorSmearer(  new SimpleClusterSmearer( _initNeutralHadronRes ), NEUTRAL_HADRON ) ;
    simpleFactory->setMomentumCut( _momentumCut ) ;
    _factory = simpleFactory ;
    log<marlin::MESSAGE>() << " SimpleFastMCProcessor::init() : registering SimpleParticleFactory " << std::endl ;
  }

  //--------------------------------------------------------------------------

  void SimpleFastMCProcessor::processEvent( EVENT::LCEvent * evt ) {

    const EVENT::LCCollection* mcpCol = evt->getCollection( _inputCollectionName ) ;
    IMPL::LCCollectionVec* recVec = new IMPL::LCCollectionVec( EVENT::LCIO::RECONSTRUCTEDPARTICLE ) ;
    UTIL::LCRelationNavigator relNav( EVENT::LCIO::RECONSTRUCTEDPARTICLE , EVENT::LCIO::MCPARTICLE ) ;

    for(int i=0 ; i<mcpCol->getNumberOfElements() ; i++ ) {
      EVENT::MCParticle* mcp = dynamic_cast<EVENT::MCParticle*> ( mcpCol->getElementAt( i ) ) ;
      // stable particles only
      if( mcp->getGeneratorStatus() == 1 ) {
        EVENT::ReconstructedParticle *rec = nullptr ;
        if( _factory != nullptr ) {
          rec = _factory->createReconstructedParticle( mcp ) ;
        }
        if( rec != nullptr ) {
          recVec->addElement( rec ) ;
          relNav.addRelation( rec , mcp ) ;
        }
      }
    }
    recVec->setDefault( true ) ;
    evt->addCollection( recVec, _recoParticleCollectionName ) ;
    evt->addCollection( relNav.createLCCollection() , _mcTruthCollectionName ) ;
  }

  // processor declaration
  MARLIN_DECLARE_PROCESSOR( SimpleFastMCProcessor )
} // end namespace
