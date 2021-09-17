import sys
import subprocess
import string
import os
import optparse
import shutil

def run(msg, cmd, shell=False, childEnv = os.environ):
    logFile.write("EXECUTING: " + " ".join(cmd) + "\n")
    logFile.flush()


    sys.stdout.write("%-70s: " % msg)
    sys.stdout.flush()
    if shell:
        ret = subprocess.Popen(" ".join(cmd), stdout=logFile, stderr=subprocess.STDOUT, env=childEnv, shell=True)
    else:
        ret = subprocess.Popen(cmd, stdout=logFile, stderr=subprocess.STDOUT, env=childEnv)
    

    if ret.wait() == 0:
        print("\033[0;32mok\033[0m")
        logFile.flush()
    else:
        print("\033[0;31mfailed\033[0m")
        logFile.close()
        os.system("tail -n 40 {}".format(logFile.name))
        sys.exit(-1)

def formatLLVMFile(names, llvmText = False):
    fileName = ".".join(names)
    if llvmText:
        return "{}.ll".format(fileName)
    else:
        return "{}.bc".format(fileName)

def clangIn(inFile, outFile, llvmText = False):
    cmd = ["clang"]
    cmd += ["-m64"]
    cmd += ["-fopenmp", "-fopenmp-targets="+targetArch+"-unknown-linux-gnu", "-fno-exceptions"]
    if llvmText:
        cmd += ["-S"]
    cmd += ["-emit-llvm"]
    cmd += ["-O2"]
    cmd += ["-o", outFile]

    for dfn in options.defines or []:
        cmd += ["-D" + dfn]

    #cmd += ["-mllvm", "-disable-licm-promotion"]

    #cmd += ["-mllvm", "-inline-threshold=100000"]

    cmd += ["-mllvm", "-pragma-unroll-threshold=4294967295"]
    #cmd += ["-ffast-math"]
    if options.vectorize:
        cmd += ["-fno-unroll-loops"]
        if options.vectorElements:
            cmd += ["-mllvm", "-force-vector-width="+options.vectorElements]
        #cmd += ["-Rpass-missed=loop-vectorize", "-Rpass-analysis=loop-vectorize"]
        #cmd += ["-mllvm", "-force-vector-width=16"]
    else:
        cmd += ["-fno-unroll-loops", "-fno-vectorize", "-fno-slp-vectorize"]
    #cmd += ["-fno-unroll-loops"]
    #cmd += ["-mprefer-vector-width=512"]
    #cmd += ["-mllvm", "-force-vector-width=64"]
    #cmd += ["-mllvm", "-force-vector-width=128"]
    #cmd += ["-mllvm", "-force-vector-width=256"]
    # ompHeaderDirectory = os.environ["NYMBLE_LLVM_HOME"]+"/projects/openmp/runtime/src"
    # ompHeaderDirectory = "/home/wasmii_jupiter/usr/tkojima/nymble/build2/projects/openmp/runtime/src"
    # ompHeaderDirectory = "/home/wasmii5/local/llvm-9.0.1/lib/clang/9.0.1/include" 
    # cmd += ["-I", ompHeaderDirectory]
    cmd += [inFile]
    run("Clang frontend - "+inFile, cmd)

def unbundle(inFile, hostFile, deviceFile, llvmText = False):
    cmd = ["clang-offload-bundler"]
    cmd += ["--inputs={}".format(inFile)]
    cmd += ["--outputs={},{}".format(hostFile, deviceFile)]
    cmd += ["--targets=host-x86_64-unknown-linux-gnu,openmp-"+targetArch+"-unknown-linux-gnu"]
    if llvmText:
        cmd += ["--type=ll"]
    else:
        cmd += ["--type=bc"]
    cmd += ["--unbundle"]

    run("OpenMP target unbundling - "+inFile, cmd)

def bundle(hostFile, deviceFile, outFile):
    cmd = ["clang-offload-bundler"]
    cmd += ["--inputs={},{}".format(hostFile, deviceFile)]
    cmd += ["--outputs={}".format(outFile)]
    cmd += ["--targets=host-x86_64-unknown-linux-gnu,openmp-x86_64-unknown-linux-gnu"]
    cmd += ["--type=o"]
    run("OpenMP target bundling", cmd)

def opt(inFile, outFile, options, msg, llvmText):
    cmd = ["opt"]
    cmd += ["-o", outFile]
    if llvmText:
        cmd += ["-S"]
    #cmd += ["-inline-threshold=1000000000"]
    cmd.extend(options)
    cmd += [inFile]
    run(msg, cmd, True)

def llc(inFile, outFile, msg, options = [], overrideTarget = ""):
    cmd = ["llc"]
    cmd += ["-o", outFile]
    if len(overrideTarget) != 0:
        cmd += ["-mtriple={}".format(overrideTarget)]
    cmd += ["-filetype=obj"]
    cmd.extend(options)
    cmd += [inFile]
    run(msg, cmd)

def llvmLink(inFiles, outFile, llvmText):
    cmd = ["llvm-link"]
    cmd += ["-o", outFile]
    if llvmText:
        cmd += ["-S"]
    cmd.extend(inFiles)
    run("llvm-link - "+outFile, cmd)

def clangOut(inFile, outFile, ASE = False):
    cmd = ["clang"]
    cmd += ["-L", llvmLibDir]
    cmd += ["-fopenmp", "-fopenmp-targets=x86_64-unknown-linux-gnu"]
    cmd += ["-L", opaeLibDir]
    cmd += ["-lnymblle_opae"]
    cmd += ["-finline-functions"]
    if ASE:
        cmd += ["-lopae-c-ase"]
    else:
        cmd += ["-lopae-c"]
    cmd += ["-ltbb"]

    for lib in options.lib_search or []:
        cmd += ["-L" + lib]

    for lib in options.libraries or []:
        cmd += ["-l" + lib]

    #cmd += ["-L/usr/lib64/atlas/", "-ltatlas"]
    cmd += ["-o", outFile]
    cmd += [inFile]
    # run("Clang - create application executable", cmd)

def hostCompile(inFile, outFile, llvmText):
    optFile = formatLLVMFile(["host", "opt"], llvmText)
    opt(inFile, optFile, ["-O3"], "opt - Optimize host code", llvmText)
    llc(optFile, outFile, "llc - Translating host code")

def deviceCompile(inFile, outFile, llvmText):
    lowerFile = formatLLVMFile(["device", "local", "mem", "lower1"], llvmText)
    lowerFile = inFile
    opt(inFile, lowerFile, ["--load-pass-plugin="+llvmLibDir+"/NymblleLocalMemLowering.so", "--passes=\"nymblle-shared-local-mem\""], "opt - Lower local memory allocations", llvmText)

    optFile = formatLLVMFile(["device", "opt"], llvmText)
    opt(lowerFile, optFile, ["-O2", "-sroa", "--relocation-model=pic", "--disable-loop-unrolling", "-mem2reg", "--disable-slp-vectorization", "-vectorize-loops=false"], "opt - Optimize device code", llvmText)

    transformFile = formatLLVMFile(["device", "transform"], llvmText)
    opt(optFile, transformFile, ["--load", llvmLibDir+"/NymblleDistributeTransform.so", "-nymblle-distribute-transform", "-deadargelim", "--relocation-model=pic"], "opt - Transform device code", llvmText)
    patchFile = formatLLVMFile(["device", "patch"], llvmText)
    opt(transformFile, patchFile, ["--load-pass-plugin="+llvmLibDir+"/NymblleCallPatcher.so", "--passes=\"nymblle-call-patcher\""], "opt - Patch device code", llvmText)
    llc(patchFile, outFile, "llc - Translating device code", ["--relocation-model=pic"], "x86_64-unknown-linux-gnu")
    syncFile = formatLLVMFile(["device", "sync"], llvmText)
    opt(transformFile, syncFile, ["--load-pass-plugin="+llvmLibDir+"/NymblleSyncHandler.so", "--passes=\"nymblle-sync-handler\""], "opt - Handle OMP sync constructs", llvmText)
    rtFile = formatLLVMFile(["device", "omp", "rt"], llvmText)
    opt(syncFile, rtFile, ["--load-pass-plugin="+llvmLibDir+"/NymblleOMPHandler.so", "--passes=\"nymblle-omp-handler\""], "opt - Handle OMP runtime calls", llvmText)
    scalarFile = formatLLVMFile(["device", "scalar"], llvmText)
    if(options.scalarizeVectorOperations):
        opt(rtFile, scalarFile, ["--scalarizer"], "opt - Scalarize vector operations", llvmText)
    else:
        scalarFile = rtFile
    lowerFile = formatLLVMFile(["device", "local", "mem", "lower2"], llvmText)
    lowerFile = scalarFile
    # opt(scalarFile, lowerFile, ["--load-pass-plugin="+llvmLibDir+"/NymblleLocalMemLowering.so", "--passes=\"nymblle-shared-local-mem\""], "opt - Lower local memory allocations", llvmText)
    partFile = formatLLVMFile(["device", "local", "mem", "part"], llvmText)
    if(options.lmemPartition):
        opt(lowerFile, partFile, ["--load-pass-plugin="+llvmLibDir+"/NymblleLocalMemPartitioner.so", "--passes=\"nymblle-local-mem-partitioner\""], "opt - Partition local memories", llvmText)
    else:
        partFile = lowerFile
    nymblleFile = formatLLVMFile(["device", "nymblle", "in"], llvmText)
    opt(partFile, nymblleFile, ["-dce"], "opt - Dead code elimination", llvmText)
    #opt(transformFile, nymblleFile, ["--load-pass-plugin="+llvmLibDir+"/NymblleSyncHandler.so", "--passes=\"nymblle-sync-handler\""], "opt - Handle OMP sync constructs", llvmText)
    #opt(nymblleFile, nymblleFile, ["--load-pass-plugin="+llvmLibDir+"/NymblleOMPHandler.so", "--passes=\"nymblle-omp-handler\""], "opt - Handle OMP runtime calls", llvmText)
    #opt(nymblleFile, nymblleFile, ["--load" ,llvmLibDir+"/NymblleTreeHeightReduction.so", "-nymblle-thr"], "opt - Tree height reduction", llvmText)
    # opt(nymblleFile, nymblleFile, ["--load-pass-plugin="+llvmLibDir+"/NymblleLocalMemHandler.so", "--passes=\"nymblle-local-mem-handler\""], "opt - Mark local memories", llvmText)

def main():
    global logFile, targetArch, llvmLibDir, opaeLibDir, options

    parser = optparse.OptionParser("Usage: driver.py [options] inputs-files.c")
    parser.add_option("-S", dest="textoutput", action="store_true", help="Store LLVM IR in human-readable representation.")
    parser.add_option("--vectorize", dest="vectorize", action="store_true", help="Vectorize loops")
    parser.add_option("--vector-elements" , dest="vectorElements", help="set number of vector elements for vectorization")
    parser.add_option("--no-scalarizer", dest="scalarizeVectorOperations", action="store_false", help="Do not scalarize vector operations", default=True)
    parser.add_option("--no-lmem-partition", dest="lmemPartition", action="store_false", help="Do not scalarize vector operations", default=True)
    parser.add_option("-D", dest="defines", action="append", help="specify preprocessor defines")
    parser.add_option("-L"                                , dest="lib_search"               , action="append"     , help="specify library directories")
    parser.add_option("-l"                                , dest="libraries"                , action="append"     , help="specify libraries")

    (options, args) = parser.parse_args()

    if len(args) == 0:
        parser.error("Not input file specified")

    llvmText = options.textoutput

    logFile = open("compilation.txt", "w")
    targetArch = "x86_64"
    llvmLibDir = os.environ["NYMBLE_LLVM_HOME"]+"/lib"
    opaeLibDir = ""
    # opaeLibDir = os.environ["NYMBLE_OPAE_HOME"]+"/lib64"

    inFiles = args

    hostFiles = []
    deviceFiles = []
    for i in inFiles:
        prefix = i.split(".", 1)[0]
        bundledFile = formatLLVMFile([prefix, "bundled"], llvmText)
        clangIn(i, bundledFile, llvmText)
        hostFile = formatLLVMFile([prefix, "host"], llvmText)
        deviceFile = formatLLVMFile([prefix, "device"], llvmText)
        unbundle(bundledFile, hostFile, deviceFile, llvmText)
        hostFiles += [hostFile]
        deviceFiles += [deviceFile]

    hostInFile = formatLLVMFile(["host", "in"], llvmText)
    llvmLink(hostFiles, hostInFile, llvmText)

    deviceInFile = formatLLVMFile(["device", "in"], llvmText)
    llvmLink(deviceFiles, deviceInFile, llvmText)


    hostOutFile = "host.o"
    deviceOutFile = "device.o"
    hostCompile(hostInFile, hostOutFile, llvmText)
    deviceCompile(deviceInFile, deviceOutFile, llvmText)
    appObjectFile = "app.o"
    bundle(hostOutFile, deviceOutFile, appObjectFile)
    clangOut(appObjectFile, "app.exe")
    clangOut(appObjectFile, "app_ase.exe", True)

    #config = os.environ.get("NYMBLLE_PCF_FILE")
    #dest = "{}/omp.pcf".format(os.getcwd())
    #shutil.copy(config, dest)

if __name__ == "__main__":
	main()
