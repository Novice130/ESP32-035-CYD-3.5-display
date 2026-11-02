// ==============================================================================
//  scenes.h - scene registry. A scene exists when its header exists.
//  Build with -DONLY_<name> (sim) to compile a single scene quickly.
// ==============================================================================
#pragma once
#include "hg_core.h"

#define HG_WANT(n) (!defined(HG_ONLY_ONE) || defined(ONLY_##n))

#if !defined(HG_ONLY_ONE) || defined(ONLY_bronze)
#if __has_include("scenes/bronze.h")
#include "scenes/bronze.h"
#define HAVE_bronze
#endif
#endif
#if !defined(HG_ONLY_ONE) || defined(ONLY_ember)
#if __has_include("scenes/ember.h")
#include "scenes/ember.h"
#define HAVE_ember
#endif
#endif
#if !defined(HG_ONLY_ONE) || defined(ONLY_water)
#if __has_include("scenes/water.h")
#include "scenes/water.h"
#define HAVE_water
#endif
#endif
#if !defined(HG_ONLY_ONE) || defined(ONLY_zen)
#if __has_include("scenes/zen.h")
#include "scenes/zen.h"
#define HAVE_zen
#endif
#endif
#if !defined(HG_ONLY_ONE) || defined(ONLY_chem)
#if __has_include("scenes/chem.h")
#include "scenes/chem.h"
#define HAVE_chem
#endif
#endif
#if !defined(HG_ONLY_ONE) || defined(ONLY_synth)
#if __has_include("scenes/synth.h")
#include "scenes/synth.h"
#define HAVE_synth
#endif
#endif
#if !defined(HG_ONLY_ONE) || defined(ONLY_cosmos)
#if __has_include("scenes/cosmos.h")
#include "scenes/cosmos.h"
#define HAVE_cosmos
#endif
#endif

static const Scene* const SCENES[] = {
#ifdef HAVE_bronze
    &SCENE_bronze,
#endif
#ifdef HAVE_ember
    &SCENE_ember,
#endif
#ifdef HAVE_water
    &SCENE_water,
#endif
#ifdef HAVE_zen
    &SCENE_zen,
#endif
#ifdef HAVE_chem
    &SCENE_chem,
#endif
#ifdef HAVE_synth
    &SCENE_synth,
#endif
#ifdef HAVE_cosmos
    &SCENE_cosmos,
#endif
};
static const uint8_t NUM_SCENES = sizeof(SCENES) / sizeof(SCENES[0]);
