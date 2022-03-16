compile
prepare dpendency

sudo apt-get install subversion build-essential \
    libncurses5-dev zlib1g-dev gawk git ccache \
    gettext libssl-dev xsltproc libxml-parser-perl \
    gengetopt default-jre-headless ocaml-nox sharutils u-boot-tools
cd slk-r680_v1
./scripts/feeds update -a
./scripts/feeds install -a -f
cp new-config .config
build make V=s -j4 || make V=s
