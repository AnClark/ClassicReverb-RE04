#pragma once

#define DISTRHO_PLUGIN_NAME    "Classic Reverb RE-04"
#define DISTRHO_PLUGIN_URI     "urn:classic-reverb:1"
#define DISTRHO_PLUGIN_BRAND   "Classic Reverb"
#define DISTRHO_PLUGIN_CLAP_ID "classic-reverb.reverb.1"

#define DISTRHO_PLUGIN_NUM_INPUTS   2
#define DISTRHO_PLUGIN_NUM_OUTPUTS  2
#define DISTRHO_PLUGIN_IS_RT_SAFE   1
#define DISTRHO_PLUGIN_WANT_TIMEPOS 0

#define DISTRHO_PLUGIN_HAS_UI          0
#define DISTRHO_PLUGIN_IS_SYNTH        0

//#define DISTRHO_PLUGIN_WANT_PROGRAMS   1
//#define DISTRHO_PLUGIN_NUM_PROGRAMS    33

/* VST2 unique ID – must be a bare 4-character token (no quotes, no commas) */
#define DISTRHO_PLUGIN_UNIQUE_ID       CRV4
