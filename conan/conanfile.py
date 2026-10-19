import glob
import os
import re
import shlex
import subprocess

from conan import ConanFile
from conan.errors import ConanException, ConanInvalidConfiguration
from conan.tools.env import Environment
from conan.tools.files import copy, get, unzip
from conan.tools.scm import Git


class OmnireflConan(ConanFile):
    name = "omnirefl"
    version = "0.0.5"
    license = "MIT"
    homepage = "https://omnirefl.io"
    url = "https://github.com/sergio-eld/omnirefl"
    description = "C++ reflection through source instrumentation"
    required_conan_version = ">=2.0"

    # Universal executables and public headers do not depend on consumer settings.
    # Keep options in the package ID so different source commits remain distinct.
    options = {"commit": [None, "ANY"]}
    default_options = {"commit": None}

    def validate(self):
        if self.options.commit:
            if not re.fullmatch(r"[0-9a-fA-F]{40}", str(self.options.commit)):
                raise ConanInvalidConfiguration("commit must be a full Git commit hash")
        elif str(self.version) not in self.conan_data["sources"]:
            raise ConanInvalidConfiguration("No prebuilt archive is recorded for this version")

    def validate_build(self):
        if self.options.commit and self.settings_build.arch != "x86_64":
            raise ConanInvalidConfiguration(
                "Source builds currently require an x86_64 build host"
            )

    def build(self):
        distribution = os.path.join(self.build_folder, "distribution")
        # Acquisition depends on options; Conan shares source() across package IDs.
        if not self.options.commit:
            get(self, **self.conan_data["sources"][str(self.version)],
                destination=distribution, strip_root=True)
            return

        Git(self).clone(self.url, target="source")
        checkout = os.path.join(self.build_folder, "source")
        Git(self, folder=checkout).checkout(str(self.options.commit))

        cosmo_build = os.path.join(self.build_folder, "cosmo-build")
        packages = os.path.join(self.build_folder, "packages")
        os.makedirs(cosmo_build, exist_ok=True)
        os.makedirs(packages, exist_ok=True)
        command = ["docker", "compose", "--file", "docker-compose.yml", "run",
                   "--rm", "--no-deps", "--volume", f"{cosmo_build}:/build"]
        # Keep Docker's outputs writable and removable by the Conan cache owner.
        if os.name != "nt":
            command.extend(["--user", f"{os.getuid()}:{os.getgid()}"])
        command.append("build-cosmo")

        environment = Environment()
        environment.define("PACKAGE_DIR", packages)
        environment.define("PACKAGE_COMMIT", str(self.options.commit)[:12])
        environment.define("COSMO_BUILD_UNIVERSAL", "true")
        with environment.vars(self).apply():
            self.run("docker compose --file docker-compose.yml pull build-cosmo",
                     cwd=checkout)
            self.run(subprocess.list2cmdline(command) if os.name == "nt"
                     else shlex.join(command), cwd=checkout)

        archives = glob.glob(os.path.join(packages, "omnirefl-cosmo-universal-*.tar.gz"))
        if len(archives) != 1:
            raise ConanException("Expected one universal archive from build-cosmo")
        unzip(self, archives[0], destination=distribution, strip_root=True)

    def package(self):
        copy(self, "*", src=os.path.join(self.build_folder, "distribution"),
             dst=self.package_folder)

    def package_info(self):
        self.cpp_info.libdirs = []
        self.cpp_info.builddirs = ["lib/cmake/omnirefl"]
        self.cpp_info.set_property("cmake_find_mode", "none")
