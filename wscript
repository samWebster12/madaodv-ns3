# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('madaodv', ['core', 'wifi',
                                               'network', 'internet',
                                               'mobility' ])
    module.source = [
        'model/madaodv-dpd.cc',
        'model/madaodv-helper.cc',
        'model/madaodv-id-cache.cc',
        'model/madaodv-neighbor.cc',
        'model/madaodv-packet.cc',
        'model/madaodv-routing-protocol.cc',
        'model/madaodv-rqueue.cc',
        'model/madaodv-rtable.cc',
        'model/hybrid-wifi-mac.cc',
        'helper/madaodv-helper.cc'
        ]

    module_test = bld.create_ns3_module_test_library('madaodv')
    module_test.source = [
        ]

    headers = bld(features='ns3header')
    headers.module = 'madaodv'
    headers.source = [
        'model/madaodv-dpd.h',
        'model/madaodv-helper.h',
        'model/madaodv-id-cache.h',
        'model/madaodv-neighbor.h',
        'model/madaodv-packet.h',
        'model/madaodv-routing-protocol.h',
        'model/madaodv-rqueue.h',
        'model/madaodv-rtable.h',
        'model/hybrid-wifi-mac.h',
        'helper/madaodv-helper.h'
        ]

    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    # Comment to disable python bindings
    # bld.ns3_python_bindings()