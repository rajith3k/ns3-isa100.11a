## -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

def configure(conf):
    
    conf.env.append_value("CXXFLAGS", [
    "-Wno-error",
    "-m64", 
    "-O0", 
    "-fPIC", 
    "-fno-strict-aliasing", 
    "-fexceptions", 
    "-DNDEBUG", 
    "-DIL_STD", 
    "-I/opt/CPLEX_Studio1271/concert/include",
    "-I/opt/CPLEX_Studio1271/cplex/include"])

    conf.env.append_value("LINKFLAGS", [
    "-L/opt/CPLEX_Studio1271/cplex/lib/x86-64_osx/static_pic",
    "-L/opt/CPLEX_Studio1271/concert/lib/x86-64_osx/static_pic",
    "-lconcert",
    "-lilocplex",
    "-lcplex", 
    "-lm", 
    "-lpthread"])

    conf.env['lconcert'] = conf.check(mandatory=True, lib='concert', uselib_store='CONCERT')
    conf.env['lilocplex'] = conf.check(mandatory=True, lib='ilocplex', uselib_store='ILOCPLEX')
    conf.env['lcplex'] = conf.check(mandatory=True, lib='cplex', uselib_store='CPLEX')
    
# Configurations for OSX, if using make sure to update the IBM paths to reflect local machine
    #conf.env.append_value("CXXFLAGS", ["-Wno-unused-private-field", "-m64", "-O", "-fPIC", "-fexceptions", "-DNDEBUG", "-DIL_STD", "-stdlib=libstdc++", "-I/opt/ibm/ILOG/ILOG/CPLEX_Studio_Community1263/cplex/include", "-I/opt/ibm/ILOG/CPLEX_Studio_Community1263/concert/include"])

    #conf.env.append_value("LINKFLAGS", ["-L/opt/ibm/ILOG/CPLEX_Studio_Community1263/cplex/lib/x86-64_osx/static_pic/", "-L/opt/ibm/ILOG/CPLEX_Studio_Community1263/    concert/lib/x86-64_osx/static_pic", "-lconcert", "-lilocplex", "-lcplex", "-m64", "-lm", "-lpthread", "-framework", "CoreFoundation", "-framework", "IOKit", "-stdlib=libstdc++"])



def build(bld):
    obj = bld.create_ns3_module('isa100-11a', ['core', 'network', 'mobility', 'spectrum', 'propagation', 'energy'])
    obj.source = [
        'model/zigbee-phy.cc',
        'model/isa100-battery.cc',
        'model/isa100-processor.cc',
        'model/isa100-sensor.cc',
        'model/isa100-dl.cc',
        'model/isa100-dl-header.cc',
        'model/isa100-dl-trailer.cc',
        'model/isa100-net-device.cc',
        'model/fish-wpan-spectrum-value-helper.cc',
        'model/fish-wpan-spectrum-signal-parameters.cc',
        'model/isa100-application.cc',
	'model/isa100-error-model.cc',
        'model/isa100-routing.cc',
        'model/fish-propagation-loss-model.cc',
	'model/zigbee-trx-current-model.cc',
	'model/tdma-optimizer-base.cc',
	'model/goldsmith-tdma-optimizer.cc',
	'model/minhop-tdma-optimizer.cc',
    	'model/convex-integer-tdma-optimizer.cc',
	'helper/isa100-helper.cc',
	'helper/isa100-helper-locations.cc',
	'helper/isa100-helper-scheduling.cc',
        ]

#    obj_test = bld.create_ns3_module_test_library('isa100-11a')
#    obj_test.source = [
#        'test/lr-wpan-pd-plme-sap-test.cc',
#        'test/lr-wpan-packet-test.cc',
#        'test/lr-wpan-error-model-test.cc',
#        'test/lr-wpan-spectrum-value-helper-test.cc',
#        ]
     
    headers = bld(features='ns3header')
    headers.module = 'isa100-11a'
    headers.source = [
        'model/zigbee-phy.h',
        'model/isa100-battery.h',
        'model/isa100-processor.h',
        'model/isa100-sensor.h',
        'model/isa100-dl.h',
        'model/isa100-dl-header.h',
        'model/isa100-dl-trailer.h',
        'model/isa100-net-device.h',
        'model/fish-wpan-spectrum-value-helper.h',
        'model/fish-wpan-spectrum-signal-parameters.h',
        'model/isa100-application.h',
        'model/isa100-error-model.h',
        'model/isa100-routing.h',
        'model/fish-propagation-loss-model.h',
	'model/zigbee-trx-current-model.h',
	'model/tdma-optimizer-base.h',
	'model/goldsmith-tdma-optimizer.h',
	'model/minhop-tdma-optimizer.h',
	'model/convex-integer-tdma-optimizer.h',
        'helper/isa100-helper.h',
        ]

    obj.use.append("CONCERT")
    obj.use.append("ILOCPLEX")
    obj.use.append("CPLEX")

#    if (bld.env['ENABLE_EXAMPLES']):
#        bld.recurse('examples')


