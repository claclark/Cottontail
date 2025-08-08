SRC = src test apps external WORKSPACE Makefile LICENSE README.md

.PHONY: building debugging testing perf FORCE

building: FORCE
	bazel build -c dbg --cxxopt="-Og" //...

debugging: FORCE
	bazel build -c dbg //...

testing: FORCE
	bazel test ...

fast: FORCE
	bazel build -c opt //... \
	  --cxxopt='-O3' \
	  --cxxopt='-march=native' \
	  --cxxopt='-DNDEBUG' \
	  --cxxopt='-ffunction-sections' \
	  --cxxopt='-fdata-sections' \
	  --linkopt='-Wl,--gc-sections' \
	  --linkopt='-Wl,-O2'

FORCE:

cottontail.tar: ${SRC}
	tar cvf cottontail.tar ${SRC}
