"""Bounded unchanged client environment and server relative-velocity observers."""
import argparse,json,os,re,shutil,tempfile
from pathlib import Path
from capture import capture,digest,ensure,require_success

HERE=Path(__file__).resolve().parent
CLIENT='af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c'
SERVER='033a127e4a25a60b038f15369c89305a3d53752242a1cff11ae964954e79ba4d'
LIBRARIES={
    'lwjgl-2.9.0.jar':'c5fb453896fb511a7f949090795c79773f6d6c92e4d13d1f3100f4b2e331471a',
    'lwjgl_util-2.9.0.jar':'474c2ca620f4147c72db9fa582993688d3c9bca5aed9d9e937ec4fa89b3c2fad',
    'jinput-2.0.5.jar':'79b5a4f5829e1a49a415711f7fb8eb5b9ad22defa72929fda2da96ec30d3f018',
    'jutils-1.0.0.jar':'c3334ff39cf0ab3b54925619101054c90098b7c733b1e7834c7b75e4c41e84a5'}
TAGS=re.compile(r'^(WATER|LAVA|FLAG|CLEAR|REL|PUBLISHED|PREFIX) ')

def executable(value):
    path=shutil.which(value) if value else None
    ensure(path,'JDK executable missing: '+str(value));return Path(path).resolve(strict=True)

def main(argv=None):
    p=argparse.ArgumentParser();p.add_argument('--client-jar',type=Path,required=True)
    p.add_argument('--server-jar',type=Path,required=True);p.add_argument('--libraries',type=Path,required=True)
    dest=p.add_mutually_exclusive_group(required=True);dest.add_argument('--output',type=Path);dest.add_argument('--work-dir',type=Path)
    p.add_argument('--java',default=shutil.which('java'));p.add_argument('--javac',default=shutil.which('javac'));a=p.parse_args(argv)
    ensure(digest(a.client_jar)==CLIENT,'client JAR pin differs');ensure(digest(a.server_jar)==SERVER,'server JAR pin differs')
    libs=[a.libraries/name for name in LIBRARIES]
    ensure(all(digest(path)==LIBRARIES[path.name] for path in libs),'client library pin differs')
    if a.work_dir is not None:
        a.work_dir.mkdir(parents=True,exist_ok=True);a.output=Path(tempfile.mkdtemp(prefix='original-environment-',dir=a.work_dir))
    else:a.output.mkdir(parents=True,exist_ok=False)
    env=os.environ.copy()
    for name in ('JAVA_TOOL_OPTIONS','_JAVA_OPTIONS','JDK_JAVA_OPTIONS'):env.pop(name,None)
    java,javac=executable(a.java),executable(a.javac)
    ensure(java.parent.samefile(javac.parent),'java and javac must resolve to one JDK bin')
    versions=[]
    for name,tool in [('java',java),('javac',javac)]:
        report=capture([str(tool),'-version'],a.output,name+'-version',10,env);require_success(report)
        versions.append(((a.output/report['stdout_file']).read_bytes()+(a.output/report['stderr_file']).read_bytes()).decode(errors='replace'))
    vm=re.search(r'version "(17\.[0-9]+\.[0-9]+)',versions[0]);compiler=re.search(r'javac (17\.[0-9]+\.[0-9]+)',versions[1])
    ensure(vm and compiler and vm.group(1)==compiler.group(1),'matching JDK17 runtime/compiler required')
    for side,jar,main_name,dependency_jars,marker in [
        ('client',a.client_jar,'EnvironmentObservation',libs,'Original entity environment'),
        ('server',a.server_jar,'RelativeServerObservation',[],'Original server relative')]:
        classes=a.output/(side+'-classes');classes.mkdir()
        sources=[HERE/'EnvironmentCases.java',HERE/(main_name+'.java')]
        classpath=os.pathsep.join(str(x.resolve()) for x in [classes,jar,*dependency_jars])
        build=capture([str(javac),'-encoding','UTF-8','-classpath',classpath,'-d',str(classes),*[str(x) for x in sources]],a.output,side+'-compile',30,env)
        require_success(build,empty_stderr=True)
        helpers={x.relative_to(classes).as_posix():digest(x) for x in classes.rglob('*.class')}
        ensure(set(helpers)=={'EnvironmentCases.class',main_name+'.class'},helpers)
        command=[str(java),'-Xint','-Xverify:all','-Xmx1g','-classpath',classpath,main_name]
        if side=='client':command.append(str((a.output/'worlds').resolve()))
        report=capture(command,a.output,side,120,env);require_success(report,empty_stderr=True)
        ensure(helpers=={x.relative_to(classes).as_posix():digest(x) for x in classes.rglob('*.class')},'helper class bytes changed')
        lines=(a.output/report['stdout_file']).read_text().splitlines()
        final=re.fullmatch(marker+r': ([1-9][0-9]*) rows',lines[-1]) if lines else None
        rows=[line for line in lines if TAGS.match(line)]
        ensure(final is not None and len(rows)==int(final.group(1)),'original incomplete rows')
        report.update(kind='original_environment_'+side,records=len(rows),java_version=versions[0],javac_version=versions[1],
            matching_jdk17=vm.group(1),java_executable_sha256=digest(java),javac_executable_sha256=digest(javac),
            jar_sha256=digest(jar),libraries={x.name:digest(x) for x in dependency_jars},
            sources={x.name:digest(x) for x in sources},helper_class_sha256=helpers,
            helper_pin_scope='Exact authored compiled class bytes, including Code attributes, retained and hashed before/after; individual helper Code hashes not separately claimed.',
            game_class_replacements=False,scope='Actual client World/Chunk/concrete constructors and methods' if side=='client' else 'Actual server constructors, relative method and null-World water/lava/clearance failure prefixes')
        (a.output/(side+'-run.json')).write_text(json.dumps(report,indent=2)+'\n',newline='\n')
        print(side,len(rows),'records; report=',a.output/(side+'-run.json'))
if __name__=='__main__':main()
