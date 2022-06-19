mkdir -p bin
echo 'building main...'
# m = 64*1024; s = 350000000; Math.floor(s/m)*m;
# emcc -s NO_EXIT_RUNTIME=1 -s TOTAL_MEMORY=52428800 -D__linux__ -s ALLOW_MEMORY_GROWTH=0 -g -s ASSERTIONS=1 \
emcc -s NO_EXIT_RUNTIME=1 -s TOTAL_MEMORY=52428800 -D__linux__ -s ALLOW_MEMORY_GROWTH=0 -O3 \
  objectize.cc \
  FastNoise.cpp util.cc vector.cc worley.cc \
  DualContouring/main.cc DualContouring/noise.cc DualContouring/vectorMath.cc DualContouring/qef.cc DualContouring/svd.cc DualContouring/density.cc DualContouring/mesh.cc DualContouring/octree.cc DualContouring/chunk.cc DualContouring/instance.cc \
  -I. \
  -o bin/dc.js
  sed -Ei 's/dc.wasm/bin\/dc.wasm/g' bin/dc.js
  echo 'let accept, reject;const p = new Promise((a, r) => {  accept = a;  reject = r;});Module.postRun = () => {  accept();};Module.waitForLoad = () => p;run();export default Module;' >> bin/dc.js
echo done

# Prevent compile window auto close after error, to see the error details. https://askubuntu.com/a/20353/1012283
# exec $SHELL
