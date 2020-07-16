ARG BASE_IMAGE="ubuntu:18.04"
FROM ${BASE_IMAGE}
ARG DEB_PACKAGE_NAME="libpostal"
ARG DEB_PACKAGE_DESC="Debian wrapping for https://github.com/openvenues/libpostal"
ARG DEB_PACKAGE_VERSION="0.0.1"
ARG DEB_PACKAGE_STUART_VERSION="1"
ARG BASE_PACKAGES="ruby ruby-dev rubygems build-essential curl autoconf automake libtool pkg-config git gettext"
ENV APT_ARGS="-y -o APT::Install-Suggests=false -o APT::Install-Recommends=false"

RUN apt-get update -qq && \
    DEBIAN_FRONTEND=noninteractive apt-get install ${APT_ARGS} ${BASE_PACKAGES}

# compile libpostal
COPY ./ /src
WORKDIR /src
RUN bash bootstrap.sh && \
    mkdir -p /output/usr && \
    ./configure --prefix=/output/usr --datadir=/usr/share/libpostal && \
    make -j && \
    make install

# move out big files from the package
WORKDIR /output
RUN tar -C / -zcf data.tgz usr/share/libpostal && \
    rm -rf /output/usr/share/libpostal/* /usr/share/libpostal

# build deb package
RUN bash /src/assets/bin/fpm-packaging
