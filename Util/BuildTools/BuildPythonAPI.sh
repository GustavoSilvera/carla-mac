#! /bin/bash

# ==============================================================================
# -- Parse arguments -----------------------------------------------------------
# ==============================================================================

DOC_STRING="Build and package CARLA Python API."

USAGE_STRING="Usage: $0 [-h|--help] [--rebuild] [--clean] [--python-version=VERSION]"

REMOVE_INTERMEDIATE=false
BUILD_RSS_VARIANT=false
BUILD_PYTHONAPI=true

OPTS=`getopt -o h --long help,config:,rebuild,clean,rss,carsim,python-version:,no-install-wheel,target-wheel-platform:,packages:,clean-intermediate,all,xml,target-archive:, -n 'parse-options' -- "$@"`

eval set -- "$OPTS"

PY_VERSION_LIST=3
TARGET_WHEEL_PLATFORM=
INSTALL_WHEEL=true

while [[ $# -gt 0 ]]; do
  case "$1" in
    --rebuild )
      REMOVE_INTERMEDIATE=true;
      BUILD_PYTHONAPI=true;
      shift ;;
    --python-version )
      PY_VERSION_LIST="$2"
      shift 2 ;;
    --no-install-wheel )
      INSTALL_WHEEL=false
      shift ;;
    --target-wheel-platform )
      TARGET_WHEEL_PLATFORM="$2"
      shift 2 ;;
    --rss )
      BUILD_RSS_VARIANT=true;
      shift ;;
    --clean )
      REMOVE_INTERMEDIATE=true;
      BUILD_PYTHONAPI=false;
      shift ;;
    -h | --help )
      echo "$DOC_STRING"
      echo "$USAGE_STRING"
      exit 1
      ;;
    * )
      shift ;;
  esac
done

source $(dirname "$0")/Environment.sh

export CC=clang-8
export CXX=clang++-8

if ! { ${REMOVE_INTERMEDIATE} || ${BUILD_PYTHONAPI} ; }; then
  fatal_error "Nothing selected to be done."
fi

# Convert comma-separated string to array of unique elements.
IFS="," read -r -a PY_VERSION_LIST <<< "${PY_VERSION_LIST}"

pushd "${CARLA_PYTHONAPI_SOURCE_FOLDER}" >/dev/null

# ==============================================================================
# -- Clean intermediate files --------------------------------------------------
# ==============================================================================

if ${REMOVE_INTERMEDIATE} ; then

  log "Cleaning intermediate files and folders."

  rm -Rf build dist source/carla.egg-info

  find source -name "*.so" -delete
  find source -name "__pycache__" -type d -exec rm -r "{}" \;

fi

# ==============================================================================
# -- Build API -----------------------------------------------------------------
# ==============================================================================

if ${BUILD_RSS_VARIANT} ; then
  export BUILD_RSS_VARIANT=${BUILD_RSS_VARIANT}
fi

if ${BUILD_PYTHONAPI} ; then
  # Add patchelf to the path. Auditwheel relies on patchelf to repair ELF files.
  export PATH="${LIBCARLA_INSTALL_CLIENT_FOLDER}/bin:${PATH}"

  for PY_VERSION in ${PY_VERSION_LIST[@]} ; do
    log "Building Python API for Python ${PY_VERSION}."

    if [[ -z ${TARGET_WHEEL_PLATFORM} ]] ; then
      /usr/bin/env python${PY_VERSION} setup.py bdist_wheel --dist-dir dist/.tmp
      cp dist/.tmp/$(ls dist/.tmp | grep .whl) dist
    else
      /usr/bin/env python${PY_VERSION} setup.py bdist_wheel --dist-dir dist/.tmp --plat ${TARGET_WHEEL_PLATFORM}
      auditwheel repair --plat ${TARGET_WHEEL_PLATFORM} --wheel-dir dist dist/.tmp/$(ls dist/.tmp | grep .whl)
    fi

    # Install wheel
    if ${INSTALL_WHEEL} ; then
      /usr/bin/env python${PY_VERSION} -m pip install --user --upgrade --force-reinstall dist/$(ls dist/.tmp | grep .whl)
    fi

    rm -rf dist/.tmp

  done

fi

# ==============================================================================
# -- ...and we are done --------------------------------------------------------
# ==============================================================================

popd >/dev/null

log "Success!"
