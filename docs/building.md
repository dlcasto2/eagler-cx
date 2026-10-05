# Building Crafti Survival Edition

## Calculator build with GitHub Actions (no setup)

Push this repository to your own GitHub fork. The **Calculator build** workflow
builds the Ndless r2020 toolchain (about 40 minutes the first time, then cached)
and uploads an artifact named `crafti-calculator` with `crafti.tns` and
`crafti-stress.tns`.

## Calculator build on Linux or WSL

```bash
sudo apt-get install -y libgmp-dev libmpfr-dev libmpc-dev libboost-program-options-dev
git clone --recursive --branch r2020 https://github.com/ndless-nspire/Ndless.git
(cd Ndless/ndless-sdk/toolchain && ./build_toolchain.sh)
export PATH="$PWD/Ndless/ndless-sdk/toolchain/install/bin:$PWD/Ndless/ndless-sdk/bin:$PATH"
make -C Ndless -j$(nproc)
make -C crafti -j$(nproc)                                   # crafti.tns
make -C crafti clean-objs && make -C crafti STRESS=1 -j$(nproc)   # crafti-stress.tns
```

## PC build and tests

```bash
sudo apt-get install -y g++ make libsdl1.2-dev zlib1g-dev
make -C tests test                     # host unit tests
make -f Makefile.pc -j$(nproc)         # crafti.elf
SDL_VIDEODRIVER=dummy CRAFTI_MAX_FRAMES=200 ./crafti.elf /tmp/smoke.map.tns
```
