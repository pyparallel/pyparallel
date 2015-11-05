"""Test the Jupyter command-line"""

import json
import os
import sys
from subprocess import check_output, CalledProcessError

import pytest
try:
    from unitteset.mock import patch
except ImportError:
    # py2
    from mock import patch

from jupyter_core import __version__
from jupyter_core.command import list_subcommands
from jupyter_core.paths import (
    jupyter_config_dir, jupyter_data_dir, jupyter_runtime_dir,
    jupyter_path, jupyter_config_path,
)


def get_jupyter_output(cmd):
    """Get output of a jupyter command"""
    if not isinstance(cmd, list):
        cmd = [cmd]
    return check_output([sys.executable, '-m', 'jupyter_core'] + cmd).decode('utf8').strip()


def assert_output(cmd, expected):
    assert get_jupyter_output(cmd) == expected


def test_config_dir():
    assert_output('--config-dir', jupyter_config_dir())


def test_data_dir():
    assert_output('--data-dir', jupyter_data_dir())


def test_runtime_dir():
    assert_output('--runtime-dir', jupyter_runtime_dir())


def test_paths():
    output = get_jupyter_output('--paths')
    for d in (jupyter_config_dir(), jupyter_data_dir(), jupyter_runtime_dir()):
        assert d in output
    for key in ('config', 'data', 'runtime'):
        assert ('%s:' % key) in output
    
    for path in (jupyter_config_path(), jupyter_path()):
        for d in path:
            assert d in output


def test_paths_json():
    output = get_jupyter_output(['--paths', '--json'])
    data = json.loads(output)
    assert sorted(data) == ['config', 'data', 'runtime']
    for key, path in data.items():
        assert isinstance(path, list)


def test_subcommand_not_given():
    with pytest.raises(CalledProcessError):
        get_jupyter_output([])


def test_help():
    output = get_jupyter_output('-h')


def test_subcommand_not_found():
    with pytest.raises(CalledProcessError):
        output = get_jupyter_output('nonexistant-subcommand')

def test_subcommand_list(tmpdir):
    a = tmpdir.mkdir("a")
    for cmd in ('jupyter-foo-bar',
                'jupyter-xyz',
                'jupyter-babel-fish'):
        a.join(cmd).write('')
    b = tmpdir.mkdir("b")
    for cmd in ('jupyter-foo',
                'jupyterstuff',
                'jupyter-yo-eyropa-ganymyde-callysto'):
        b.join(cmd).write('')
    
    path = os.pathsep.join(map(str, [a, b]))
    
    with patch.dict('os.environ', {'PATH': path}):
        subcommands = list_subcommands()
        assert subcommands == [
            'babel-fish',
            'foo',
            'xyz',
            'yo-eyropa-ganymyde-callysto',
        ]
