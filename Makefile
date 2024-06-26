SRC = src test apps external WORKSPACE Makefile LICENSE README.md

building: FORCE
	bazel build -c dbg --cxxopt="-Og" ...

debugging: FORCE
	bazel build -c dbg ...

testing: FORCE
	bazel test ...

FORCE:

cottontail.tar: ${SRC}
	tar cvf cottontail.tar ${SRC}
