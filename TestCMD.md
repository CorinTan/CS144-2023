## 运行全部测试
cd ~/code/CS144/minnow

cmake -S . -B build 

cmake --build build

cmake --build build --target check3

cmake --build build --target check5


## 运行单个测试
cd ~/code/CS144/minnow/build

ctest -R 测试名字

ctest -R  send_extra --rerun-failed --output-on-failure