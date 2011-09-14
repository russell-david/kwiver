/*ckwg +5
 * Copyright 2011 by Kitware, Inc. All Rights Reserved. Please refer to
 * KITWARE_LICENSE.TXT for licensing information, or contact General Counsel,
 * Kitware, Inc., 28 Corporate Drive, Clifton Park, NY 12065.
 */

#include "process.h"
#include "process_exception.h"

#include "config.h"
#include "datum.h"
#include "edge.h"
#include "stamp.h"
#include "types.h"

#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

#include <map>
#include <utility>

/**
 * \file process.cxx
 *
 * \brief Implementation of the base class for \link vistk::process processes\endlink.
 */

namespace vistk
{

process::port_t const process::port_heartbeat = port_t("heartbeat");
config::key_t const process::config_name = config::key_t("_name");
config::key_t const process::config_type = config::key_t("_type");
process::port_type_t const process::type_any = port_type_t("_any");
process::port_type_t const process::type_none = port_type_t("_none");
process::port_flag_t const process::flag_output_const = port_flag_t("_const");
process::port_flag_t const process::flag_input_mutable = port_flag_t("_mutable");
process::port_flag_t const process::flag_required = port_flag_t("_required");

process::port_info
::port_info(port_type_t const& type_,
            port_flags_t const& flags_,
            port_description_t const& description_)
  : type(type_)
  , flags(flags_)
  , description(description_)
{
}

process::port_info
::~port_info()
{
}

process::conf_info
::conf_info(config::value_t const& def_,
            config::description_t const& description_)
  : def(def_)
  , description(description_)
{
}

process::conf_info
::~conf_info()
{
}

process::data_info
::data_info(bool same_color_,
            bool in_sync_,
            datum::type_t max_status_)
  : same_color(same_color_)
  , in_sync(in_sync_)
  , max_status(max_status_)
{
}

process::data_info
::~data_info()
{
}

class process::priv
{
  public:
    priv(config_t c);
    ~priv();

    void run_heartbeat();
    bool connect_input_port(port_t const& port, edge_ref_t edge);
    bool connect_output_port(port_t const& port, edge_ref_t edge);

    name_t name;
    process_registry::type_t type;

    typedef std::map<port_t, port_info_t> port_map_t;
    typedef std::map<config::key_t, conf_info_t> conf_map_t;

    typedef std::map<port_t, edge_ref_t> input_edge_map_t;
    typedef std::map<port_t, edge_group_t> output_edge_map_t;

    port_map_t input_ports;
    port_map_t output_ports;

    conf_map_t config_keys;

    input_edge_map_t input_edges;
    output_edge_map_t output_edges;

    config_t const conf;

    ports_t required_inputs;
    ports_t required_outputs;

    bool initialized;
    bool is_complete;

    bool input_same_color;
    bool input_sync;
    bool input_valid;

    stamp_t hb_stamp;

    static config::value_t const default_name;
};

config::value_t const process::priv::default_name = "(unnamed)";

void
process
::init()
{
  if (d->initialized)
  {
    throw reinitialization_exception(d->name);
  }

  d->initialized = true;

  _init();
}

void
process
::step()
{
  if (!d->initialized)
  {
    throw uninitialized_exception(d->name);
  }

  /// \todo Make reentrant.

  /// \todo Are there any pre-_step actions?

  if (d->is_complete)
  {
    /// \todo What exactly should be done here?
  }
  else
  {
    _step();
  }

  /// \todo Are there any post-_step actions?

  d->run_heartbeat();
}

bool
process
::is_reentrant() const
{
  return false;
}

void
process
::connect_input_port(port_t const& port, edge_t edge)
{
  if (!edge)
  {
    throw null_edge_port_connection_exception(d->name, port);
  }

  if (d->initialized)
  {
    throw connect_to_initialized_process_exception(d->name, port);
  }

  edge_ref_t const ref = edge_ref_t(edge);

  if (!d->connect_input_port(port, ref))
  {
    _connect_input_port(port, ref);
  }
}

void
process
::connect_output_port(port_t const& port, edge_t edge)
{
  if (!edge)
  {
    throw null_edge_port_connection_exception(d->name, port);
  }

  edge_ref_t const ref = edge_ref_t(edge);

  if (!d->connect_output_port(port, ref))
  {
    _connect_output_port(port, ref);
  }
}

process::ports_t
process
::input_ports() const
{
  ports_t ports = _input_ports();

  BOOST_FOREACH (priv::port_map_t::value_type const& port, d->input_ports)
  {
    ports.push_back(port.first);
  }

  return ports;
}

process::ports_t
process
::output_ports() const
{
  ports_t ports = _output_ports();

  BOOST_FOREACH (priv::port_map_t::value_type const& port, d->output_ports)
  {
    ports.push_back(port.first);
  }

  return ports;
}

process::port_info_t
process
::input_port_info(port_t const& port)
{
  return _input_port_info(port);
}

process::port_info_t
process
::output_port_info(port_t const& port)
{
  return _output_port_info(port);
}

config::keys_t
process
::available_config() const
{
  config::keys_t keys = _available_config();

  BOOST_FOREACH (priv::conf_map_t::value_type const& conf, d->config_keys)
  {
    keys.push_back(conf.first);
  }

  return keys;
}

process::conf_info_t
process
::config_info(config::key_t const& key)
{
  return _config_info(key);
}

process::name_t
process
::name() const
{
  return d->name;
}

process_registry::type_t
process
::type() const
{
  return d->type;
}

process
::process(config_t const& config)
{
  if (!config)
  {
    throw null_process_config_exception();
  }

  d.reset(new priv(config));

  declare_configuration_key(config_name, boost::make_shared<conf_info>(
    priv::default_name,
    config::description_t("The name of the process.")));
  declare_configuration_key(config_type, boost::make_shared<conf_info>(
    config::value_t(),
    config::description_t("The type of the process.")));

  d->name = config_value<name_t>(config_name);
  d->type = config_value<process_registry::type_t>(config_type);

  declare_output_port(port_heartbeat, boost::make_shared<port_info>(
    type_none,
    port_flags_t(),
    port_description_t("Outputs the heartbeat stamp with an empty datum.")));
}

process
::~process()
{
}

void
process
::_init()
{
}

void
process
::_step()
{
}

void
process
::_connect_input_port(port_t const& port, edge_ref_t edge)
{
  if (!d->connect_input_port(port, edge))
  {
    throw no_such_port_exception(d->name, port);
  }
}

void
process
::_connect_output_port(port_t const& port, edge_ref_t edge)
{
  if (!d->connect_output_port(port, edge))
  {
    throw no_such_port_exception(d->name, port);
  }
}

process::ports_t
process
::_input_ports() const
{
  return ports_t();
}

process::ports_t
process
::_output_ports() const
{
  return ports_t();
}

process::port_info_t
process
::_input_port_info(port_t const& port)
{
  priv::port_map_t::iterator i = d->input_ports.find(port);

  if (i != d->input_ports.end())
  {
    return i->second;
  }

  throw no_such_port_exception(d->name, port);
}

process::port_info_t
process
::_output_port_info(port_t const& port)
{
  priv::port_map_t::iterator i = d->output_ports.find(port);

  if (i != d->output_ports.end())
  {
    return i->second;
  }

  throw no_such_port_exception(d->name, port);
}

config::keys_t
process
::_available_config() const
{
  return config::keys_t();
}

process::conf_info_t
process
::_config_info(config::key_t const& key)
{
  priv::conf_map_t::iterator i = d->config_keys.find(key);

  if (i != d->config_keys.end())
  {
    return i->second;
  }

  throw unknown_configuration_value_exception(d->name, key);
}

void
process
::declare_input_port(port_t const& port, port_info_t const& info)
{
  d->input_ports[port] = info;

  port_flags_t const& flags = info->flags;
  port_flags_t::const_iterator const i = flags.find(flag_required);

  if (i != flags.end())
  {
    d->required_inputs.push_back(port);
  }
}

void
process
::declare_output_port(port_t const& port, port_info_t const& info)
{
  d->output_ports[port] = info;

  port_flags_t const& flags = info->flags;
  port_flags_t::const_iterator const i = flags.find(flag_required);

  if (i != flags.end())
  {
    d->required_outputs.push_back(port);
  }
}

void
process
::declare_configuration_key(config::key_t const& key,conf_info_t const& info)
{
  d->config_keys[key] = info;
}

void
process
::mark_as_complete()
{
  d->is_complete = true;

  // Indicate to input edges that we are complete.
  BOOST_FOREACH (priv::input_edge_map_t::value_type& port_edge, d->input_edges)
  {
    edge_t edge = port_edge.second.lock();

    edge->mark_downstream_as_complete();
  }
}

stamp_t
process
::heartbeat_stamp() const
{
  return d->hb_stamp;
}

edge_ref_t
process
::input_port_edge(port_t const& port) const
{
  priv::port_map_t::iterator i = d->input_ports.find(port);

  if (i == d->input_ports.end())
  {
    throw no_such_port_exception(d->name, port);
  }

  priv::input_edge_map_t::iterator e = d->input_edges.find(port);

  if (e == d->input_edges.end())
  {
    return edge_ref_t();
  }

  return e->second;
}

edge_group_t
process
::output_port_edges(port_t const& port) const
{
  priv::port_map_t::iterator i = d->output_ports.find(port);

  if (i == d->output_ports.end())
  {
    throw no_such_port_exception(d->name, port);
  }

  priv::output_edge_map_t::iterator e = d->output_edges.find(port);

  if (e == d->output_edges.end())
  {
    return edge_group_t();
  }

  return e->second;
}

edge_datum_t
process
::grab_from_port(port_t const& port) const
{
  priv::port_map_t::iterator i = d->input_ports.find(port);

  if (i == d->input_ports.end())
  {
    throw no_such_port_exception(d->name, port);
  }

  priv::input_edge_map_t::iterator e = d->input_edges.find(port);

  if (e == d->input_edges.end())
  {
    static std::string const reason = "Data was requested from the port";

    throw missing_connection_exception(d->name, port, reason);
  }

  return grab_from_edge_ref(e->second);
}

void
process
::push_to_port(port_t const& port, edge_datum_t const& dat) const
{
  priv::port_map_t::iterator i = d->output_ports.find(port);

  if (i == d->output_ports.end())
  {
    throw no_such_port_exception(d->name, port);
  }

  priv::output_edge_map_t::iterator e = d->output_edges.find(port);

  if (e != d->output_edges.end())
  {
    push_to_edges(e->second, dat);
  }
}

config_t
process
::get_config() const
{
  return d->conf;
}

void
process
::ensure_inputs_are_same_color(bool ensure)
{
  d->input_same_color = ensure;
}

void
process
::ensure_inputs_are_in_sync(bool ensure)
{
  d->input_sync = ensure;
}

void
process
::ensure_inputs_are_valid(bool ensure)
{
  d->input_valid = ensure;
}

process::data_info_t
process
::edge_data_info(edge_data_t const& data)
{
  bool same_color = true;
  bool in_sync = true;
  datum::type_t max_type = datum::invalid;

  edge_datum_t const& fst = data[0];
  stamp_t const& st = fst.get<1>();

  BOOST_FOREACH (edge_datum_t const& dat, data)
  {
    datum::type_t const type = dat.get<0>()->type();

    if (max_type < type)
    {
      max_type = type;
    }

    stamp_t const& st2 = dat.get<1>();

    if (!st->is_same_color(st2))
    {
      same_color = false;
    }
    if (*st != *st2)
    {
      in_sync = false;
    }
  }

  return boost::make_shared<data_info>(same_color, in_sync, max_type);
}

void
process
::push_to_edges(edge_group_t const& edges, edge_datum_t const& dat)
{
  BOOST_FOREACH (edge_ref_t const& e, edges)
  {
    edge_t const cur_edge = e.lock();

    cur_edge->push_datum(dat);
  }
}

edge_datum_t
process
::grab_from_edge_ref(edge_ref_t const& edge)
{
  edge_t const cur_edge = edge.lock();

  return cur_edge->get_datum();
}

config::value_t
process
::config_value_raw(config::key_t const& key) const
{
  priv::conf_map_t::const_iterator const i = d->config_keys.find(key);

  if (i == d->config_keys.end())
  {
    throw unknown_configuration_value_exception(d->name, key);
  }

  if (d->conf->has_value(key))
  {
    return d->conf->get_value<config::value_t>(key);
  }

  return i->second->def;
}

process::priv
::priv(config_t c)
  : conf(c)
  , initialized(false)
  , is_complete(false)
  , input_same_color(true)
  , input_sync(true)
  , input_valid(true)
  , hb_stamp(stamp::new_stamp())
{
}

process::priv
::~priv()
{
}

void
process::priv
::run_heartbeat()
{
  datum_t dat;

  if (is_complete)
  {
    dat = datum::complete_datum();
  }
  else
  {
    dat = datum::empty_datum();
  }

  edge_datum_t const edge_dat(dat, hb_stamp);

  push_to_edges(output_edges[port_heartbeat], edge_dat);

  hb_stamp = stamp::incremented_stamp(hb_stamp);
}

bool
process::priv
::connect_input_port(port_t const& port, edge_ref_t edge)
{
  port_map_t::iterator i = input_ports.find(port);

  if (i != input_ports.end())
  {
    if (!input_edges[port].expired())
    {
      throw port_reconnect_exception(name, port);
    }

    input_edges[port] = edge;

    return true;
  }

  return false;
}

bool
process::priv
::connect_output_port(port_t const& port, edge_ref_t edge)
{
  port_map_t::iterator i = output_ports.find(port);

  if (i != output_ports.end())
  {
    output_edges[port].push_back(edge);

    return true;
  }

  return false;
}

}
