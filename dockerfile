FROM ubuntu:20.04

ENV TZ=Europe/Stockholm

RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN apt-get update
RUN apt-get install wget pkg-config devscripts debconf debhelper \
automake bison ca-certificates libcurl4-openssl-dev debhelper \
libaio-dev libtool libz-dev libgcrypt-dev libev-dev lsb-release \
build-essential rsync libdbd-mysql-perl libnuma1 socat librtmp-dev \
libtinfo5 liblz4-tool liblz4-1 liblz4-dev vim-common cmake gcc make \
dirmngr wget flex autoconf mysql-client libncurses-dev zlib1g-dev \
libnuma-dev openssl libssl-dev libgcrypt20-dev build-essential \
checkinstall zlib1g-dev ncat libudev-dev -y


RUN apt install build-essential checkinstall zlib1g-dev -y
RUN wget -P /usr/local/src/ https://www.openssl.org/source/openssl-1.1.1q.tar.gz                                                            
WORKDIR /usr/local/src/
RUN tar -xf openssl-1.1.1q.tar.gz
WORKDIR /usr/local/src/openssl-1.1.1q
RUN ./config shared enable-ec_nistp_64_gcc_128 -Wl,-rpath=/usr/local/ssl/lib --prefix=/usr/local/ssl
RUN make -j 7
RUN make -j 7 TESTS='-test_ca test_ssl_new'
RUN make -j 7 install_sw
RUN hash -r
RUN ln -sf /usr/local/ssl/bin/openssl /usr/local/bin/openssl

RUN mkdir /boost
RUN echo $(ls ${WORKSPACE})
COPY ${WORKSPACE} /compile_xtrabackup/percona-xtrabackup

WORKDIR /compile_xtrabackup/percona-xtrabackup/build

RUN cmake .. -DWITH_MAN_PAGES=OFF -DWITH_NUMA=0 -DDOWNLOAD_BOOST=1 -DWITH_BOOST=/boost -DWITH_NUMA=0 -DCMAKE_INSTALL_PREFIX=/compile_xtrabackup/percona-xtrabackup/build -DBUILD_CONFIG=xtrabackup_release
RUN make -j 4
RUN echo $(ls /compile_xtrabackup/percona-xtrabackup/build/runtime_output_directory/)

WORKDIR /compile_xtrabackup/percona-xtrabackup/build
RUN make install
RUN chmod g+rwX /bin/xtrabackup

VOLUME [/backup]
CMD ["/usr/bin/xtrabackup"]