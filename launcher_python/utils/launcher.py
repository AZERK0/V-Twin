import subprocess
import config

def launch_engine(engine_path, mode):

    cmd = [
        config.ENGINE_SIM_EXEC,
        "--scripts",
        engine_path
    ]

    print("Launching:", cmd)

    subprocess.Popen(cmd)