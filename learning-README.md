## start


### installl


#### install python & pip package

```
# build & install
$ ./configure --enable-shared --enable-unicode=ucs4 --prefix=/opt/python/Python-3.8.3 --enable-optimizations
$ make -j4 & make install

# set env -> PATH

# install pip package
$ pip3 install pyyaml
$ pip3 install graphviz
$ pip3 install pydot
$ pip3 install pydot-ng
```


### build

```
$ /usr/bin/python3 /usr/bin/scons build/X86/gem5.opt -j9
```

### run test

```
$ ./build/X86/gem5.opt configs/tutorial/test.py
```
