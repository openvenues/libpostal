FROM ubuntu:18.04
ARG DEB_PACKAGE_NAME="libpostal"
ARG DEB_PACKAGE_DESC="Debian wrapping for https://github.com/openvenues/libpostal"
ARG DEB_PACKAGE_VERSION="0.0.1"
ARG BASE_PACKAGES="ruby ruby-dev rubygems build-essential curl autoconf automake libtool pkg-config git gettext"
ENV APT_ARGS="-y -o APT::Install-Suggests=false -o APT::Install-Recommends=false"

RUN apt-get update -qq && \
    DEBIAN_FRONTEND=noninteractive apt-get install ${APT_ARGS} ${BASE_PACKAGES} && \
    gem install --no-document fpm
COPY ./ /src
WORKDIR /src
RUN bash bootstrap.sh && \
    mkdir -p /output/usr && \
    ./configure --prefix=/output/usr && \
    make -j && \
    make install
# move out big files from the package
WORKDIR /output
RUN tar -zcf address_parser.tgz usr/share/libpostal/address_parser && \
    rm -rf /output/usr/share/libpostal/address_parser
# build deb package
RUN export DEB_PACKAGE_VERSION=$(sed 's|^v||' versions/base_data)+git$(git rev-parse HEAD | head -c7) && \
    envsubst '${DEB_PACKAGE_VERSION}' < /src/assets/fpm-deb-scripts/postinst.sh.tpl > /src/assets/fpm-deb-scripts/postinst.sh && \
    fpm -n ${DEB_PACKAGE_NAME} \
        -v ${DEB_PACKAGE_VERSION} \
        --description "${DEB_PACKAGE_DESC}" \
        -s dir \
        -t deb \
        --after-install /src/assets/fpm-deb-scripts/postinst.sh \
        /output/usr=/
