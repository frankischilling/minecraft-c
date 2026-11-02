"""Build the optional Windows frame observer from an existing Ninja/MSVC SDK build.

The configured mc_test_workbench_open_shutdown target supplies the exact C17
compiler options, native SDK support objects and dependency closure. This
helper compiles only the benchmark source and links a separate executable.
It does not configure, rebuild or change the supplied build directory.
"""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def windows_arguments(command):
    """Use the native parser so quoted compiler/include paths remain intact."""
    count = ctypes.c_int()
    parse = ctypes.windll.shell32.CommandLineToArgvW
    parse.argtypes = (ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int))
    parse.restype = ctypes.POINTER(ctypes.c_wchar_p)
    values = parse(command, ctypes.byref(count))
    if not values:
        raise OSError('CommandLineToArgvW failed')
    try:
        return [values[index] for index in range(count.value)]
    finally:
        ctypes.windll.kernel32.LocalFree(ctypes.cast(values, ctypes.c_void_p))


def build_observer(build, out):
    if os.name != 'nt':
        raise RuntimeError('This SDK frame observer requires Windows and an MSVC developer shell')
    build = build.resolve(strict=True)
    out.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='client-frames-build-', dir=out.resolve()))
    common = output / 'common'
    common.mkdir()
    ninja = shutil.which('ninja')
    if ninja is None or not (build / 'build.ninja').is_file():
        raise RuntimeError('Supply a built Ninja/MSVC Release directory and Ninja on PATH')
    lines = (build / 'build.ninja').read_text().splitlines()
    target = 'mc_test_workbench_open_shutdown.exe'
    begin = next((i for i, line in enumerate(lines) if line.startswith('build ' + target + ':')), None)
    if begin is None:
        raise RuntimeError('Configure MC_ENABLE_CLIENT_WORKBENCH_OPEN_SHUTDOWN_TESTS with local '
                           'imported resources, then build mc_test_workbench_open_shutdown first')
    commands = subprocess.check_output([ninja, '-C', str(build), '-t', 'commands', target],
                                       text=True).splitlines()
    compile_line = next((line for line in commands if 'cl.exe' in line and
                         line.rstrip().rstrip('"').endswith('test_workbench_open_shutdown.c')), None)
    if compile_line is None:
        raise RuntimeError('The configured SDK target must use an available MSVC C17 compiler')
    compile_args = windows_arguments(compile_line)
    source = ROOT / 'tests/benchmarks/client_frames.c'
    source_before = digest(source)
    compile_args[-1] = str(source)
    compile_args = [('/Fo' + str(common / 'observer.obj')) if arg.startswith('/Fo') else
                    ('/Fd' + str(common) + '\\') if arg.startswith('/Fd') else arg
                    for arg in compile_args]
    if '/O2' not in compile_args or '/DNDEBUG' not in compile_args:
        raise RuntimeError('Use a Release build for this observer')
    objects = lines[begin].split(': ', 1)[1].split(' |', 1)[0].split()[1:]
    if len(objects) != 5 or not objects[0].endswith('test_workbench_open_shutdown.c.obj'):
        raise RuntimeError('The configured SDK target support-object layout changed')
    variables = {}
    for line in lines[begin + 1:]:
        if not line.strip():
            break
        key, value = line.strip().split(' = ', 1)
        variables[key] = value
    shared_objects = [str(common / 'observer.obj')]
    support_hashes = {}
    for index, relative in enumerate(objects[1:]):
        original = build / relative
        copied = common / ('sdk-' + str(index) + '.obj')
        before = digest(original)
        shutil.copy2(original, copied)
        if digest(original) != before or digest(copied) != before:
            raise RuntimeError('SDK object changed while freezing build inputs')
        shared_objects.append(str(copied))
        support_hashes[relative] = before
    libraries, library_hashes = [], {}
    for value in windows_arguments(variables['LINK_LIBRARIES']):
        original = build / value
        if original.is_file():
            copied = common / Path(value.replace('\\', '/')).name
            before = digest(original)
            if copied.exists() and digest(copied) != before:
                raise RuntimeError('Distinct SDK archives share one output basename')
            if not copied.exists():
                shutil.copy2(original, copied)
            if digest(original) != before or digest(copied) != before:
                raise RuntimeError('SDK dependency changed while freezing build inputs')
            library_hashes[value] = before
            libraries.append(str(copied))
        else:
            libraries.append(value)  # Native system import-library name.
    with (common / 'compile.log').open('wb') as log:
        compiled = subprocess.run(compile_args, cwd=build, stdout=log, stderr=subprocess.STDOUT)
    if compiled.returncode:
        raise RuntimeError('Observer compilation failed; inspect ' + str(common / 'compile.log'))
    link_line = next(line for line in reversed(commands) if '--msvc-ver=' in line and 'link.exe' in line)
    msvc = re.search(r'--msvc-ver=(\d+)', link_line).group(1)
    def option(name):
        value = re.search(re.escape(name) + r'=("[^"]+"|\S+)', link_line).group(1)
        return value.strip('"')
    linker = re.search(r' -- ("[^"]*link\.exe"|\S+link\.exe) /nologo', link_line).group(1).strip('"')
    response = output / 'observer.rsp'
    response.write_text(subprocess.list2cmdline(shared_objects + libraries) + '\n', encoding='utf-8')
    executable = output / 'client_frames.exe'
    link = ['cmake', '-E', 'vs_link_exe', '--msvc-ver=' + msvc, '--intdir=' + str(output),
            '--rc=' + option('--rc'), '--mt=' + option('--mt'), '--manifests', '--', linker,
            '/nologo', '@' + str(response), '/out:' + str(executable),
            '/implib:' + str(output / 'client_frames.lib'), '/pdb:' + str(output / 'client_frames.pdb'),
            '/version:0.0', *windows_arguments(variables['LINK_FLAGS'])]
    with (output / 'link.log').open('wb') as log:
        linked = subprocess.run(link, cwd=build, stdout=log, stderr=subprocess.STDOUT)
    if linked.returncode:
        raise RuntimeError('Observer link failed; inspect ' + str(output / 'link.log'))
    audio = build / 'OpenAL32.dll'
    shutil.copy2(audio, output / audio.name)
    if digest(source) != source_before:
        raise RuntimeError('Observer source changed during compilation')
    report = {'status': 'BUILT_NOT_RUN', 'build_directory': str(build), 'executable': str(executable),
              'source_sha256': digest(source), 'executable_sha256': digest(executable),
              'compile': compile_args, 'link': link, 'support_object_sha256': support_hashes,
              'dependency_library_sha256': library_hashes,
              'openal_dll_sha256': digest(output / audio.name),
              'scope': 'Windows x64 MSVC Release observer; no client process has run yet.'}
    (output / 'build.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return executable, output / 'build.json'
