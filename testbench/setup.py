from setuptools import setup, find_packages
from os import path

here = path.abspath(path.dirname(__file__))

# Get the long description from the README file
with open(path.join(here, "README.md"), encoding="utf-8") as f:
    long_description = f.read()

setup(
    name="variod-testbench",
    version="0.1.0.dev0",
    description="Testing utilities for Openvario variod daemon",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/Openvario/variod",
    author="Andrey Lebedev",
    author_email="andrey.lebedev@gmail.com",
    classifiers=[
        "Development Status :: 3 - Alpha",
        "License :: OSI Approved :: GNU Lesser General Public License v3 (LGPLv3)",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
    ],
    keywords="openvario variod testing",
    # package_dir={"": "src"},
    packages=find_packages(where="src"),  # Required
    python_requires=">=3.7, <4",
    install_requires=[],
    extras_require={"dev": ["black", "mypy"], "test": [],},
    entry_points={"console_scripts": ["sensord-sim=variodtb.sensordsim:main"],},
    project_urls={},
)
