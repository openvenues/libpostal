#!/bin/sh

download_data() {
  if [ -e /etc/lsb-release ]; then . /etc/lsb-release; fi
  if [ -e /etc/os-release ]; then . /etc/os-release; fi
  if [ "${DISTRIB_ID}" = "Ubuntu" ]
  then
    curl -sL https://github.com/StuartApp/libpostal/releases/download/${GH_RELEASE}/ubuntu_${DISTRIB_RELEASE}-data.tgz | tar -C / -zxf -
  else
    # we assume it's debian
    curl -sL https://github.com/StuartApp/libpostal/releases/download/${GH_RELEASE}/debian_${VERSION_ID}-data.tgz | tar -C / -zxf -
  fi
}

after_upgrade() {
  echo "Upgrading data package '${GH_RELEASE}' from GH releases..."
  download_data
}

after_install() {
  echo "Installing data package '${GH_RELEASE}' from GH releases..."
  download_data
  # workaround ruby_postal not using default include path
  ln -nfs /usr/include/libpostal /usr/local/include/libpostal
}

export PACKAGE_VERSION=${DEB_PACKAGE_VERSION}
export GH_RELEASE=$(echo ${PACKAGE_VERSION} | sed 's|-.+git|-|')

if [ "${1}" = "configure" -a -z "${2}" ] || \
   [ "${1}" = "abort-remove" ]
then
    # "after install" here
    # "abort-remove" happens when the pre-removal script failed.
    #   In that case, this script, which should be idemptoent, is run
    #   to ensure a clean roll-back of the removal.
    installingVersion="${2}"
    after_install 
elif [ "${1}" = "configure" -a -n "${2}" ]
then
    upgradeFromVersion="${2}"
    # "after upgrade" here
    # NOTE: This slot is also used when deb packages are removed,
    # but their config files aren't, but a newer version of the
    # package is installed later, called "Config-Files" state.
    # basically, that still looks a _lot_ like an upgrade to me.
    after_upgrade "${2}"
elif echo "${1}" | grep -E -q "(abort|fail)"
then
    echo "Failed to install before the post-installation script was run." >&2
    exit 1
fi
