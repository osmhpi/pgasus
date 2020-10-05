FROM ubuntu:bionic

# Install dependencies
RUN DEBIAN_FRONTEND=noninteractive apt-get update \
    && apt-get install --yes --no-install-recommends \
        build-essential \
        git \
        cmake \
        wget \
        zlib1g-dev \
        libhwloc-common \
        libhwloc-dev \
        ca-certificates

# Download, compile and install Boost 1.60
RUN wget -O /tmp/boost_1_60_0.tar.gz https://sourceforge.net/projects/boost/files/boost/1.60.0/boost_1_60_0.tar.gz/download \
	&& cd /tmp/ \
	&& tar -xzf boost_1_60_0.tar.gz \
	&& cd /tmp/boost_1_60_0 \
	&& ./bootstrap.sh --with-libraries=context,chrono,thread,system \
	&& ./b2 \
	&& ./b2 install \
	&& cd /tmp \
	&& rm -Rf /tmp/boost_1_60_0

# PGASUS
RUN git clone https://github.com/osmhpi/pgasus /tmp/pgasus --recursive \
	&& mkdir /tmp/pgasus/build \
	&& cd /tmp/pgasus/build \
	&& cmake .. \
	&& make -j$(nproc) \ 
	&& make install 