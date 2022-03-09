# coding=utf-8
# Copyright (C) 2012, 2014-2016, 2019 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation
# files (the "Software"), to deal in the Software without
# restriction, including without limitation the rights to use,
# copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following
# conditions:
#
# This permission notice shall be included in all copies or
# substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
# PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHOR(S) BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
# AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
# OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

""" This module enables running shader tests. """

import io
import os
import re

from framework import exceptions
from framework import status
from framework import options
from .base import ReducedProcessMixin, TestIsSkip
from .opengl import FastSkipMixin, FastSkip
from .piglit_test import PiglitBaseTest, ROOT_DIR

__all__ = [
    'ShaderTest',
]


class Parser(object):
    """An object responsible for parsing a shader_test file."""

    _is_gl = re.compile(r'GL (<|<=|=|>=|>) \d\.\d')
    _match_gl_version = re.compile(
        r'^GL\s+(?P<profile>(ES|CORE|COMPAT))?\s*(?P<op>(<|<=|=|>=|>))\s*(?P<ver>\d\.\d)')
    _match_glsl_version = re.compile(
        r'^GLSL\s+(?P<es>ES)?\s*(?P<op>(<|<=|=|>=|>))\s*(?P<ver>\d\.\d+)')

    def __init__(self, filename):
        self.filename = filename
        self.extensions = set()
        self.api_version = 0.0
        self.shader_version = 0.0
        self.api = None
        self.prog = None
        self.__op = None
        self.__sl_op = None

    def parse(self):
        # Iterate over the lines in shader file looking for the config section.
        # By using a generator this can be split into two for loops at minimal
        # cost. The first one looks for the start of the config block or raises
        # an exception. The second looks for the GL version or raises an
        # exception
        with io.open(os.path.join(ROOT_DIR, self.filename), 'r', encoding='utf-8') as shader_file:
            lines = (l for l in shader_file.readlines())

            # Find the config section
            for line in lines:
                # We need to find the first line of the configuration file, as
                # soon as we do then we can move on to getting the
                # configuration. The first line needs to be parsed by the next
                # block.
                if line.startswith('[require]'):
                    break
            else:
                raise exceptions.PiglitFatalError(
                    "In file {}: Config block not found".format(self.filename))

        for line in lines:
            if line.startswith('GL_'):
                line = line.strip()
                if not (line.startswith('GL_MAX') or line.startswith('GL_NUM')):
                    self.extensions.add(line)
                    if line == 'GL_ARB_compatibility':
                        assert self.api is None or self.api == 'compat'
                        self.api = 'compat'
                continue

            # Find any GLES requirements.
            if not self.api_version:
                m = self._match_gl_version.match(line)
                if m:
                    self.__op = m.group('op')
                    self.api_version = float(m.group('ver'))
                    if m.group('profile') == 'ES':
                        assert self.api is None or self.api == 'gles2'
                        self.api = 'gles2'
                    elif m.group('profile') == 'COMPAT':
                        assert self.api is None or self.api == 'compat'
                        self.api = 'compat'
                    elif self.api_version >= 3.1:
                        assert self.api is None or self.api == 'core'
                        self.api = 'core'
                    continue

            if not self.shader_version:
                # Find any GLSL requirements
                m = self._match_glsl_version.match(line)
                if m:
                    self.__sl_op = m.group('op')
                    self.shader_version = float(m.group('ver'))
                    if m.group('es'):
                        assert self.api is None or self.api == 'gles2'
                        self.api = 'gles2'
                    continue

            if line.startswith('['):
                if not self.api:
                    # Because this is inferred rather than explicitly declared
                    # check this after al other requirements are parsed. It's
                    # possible that a test can declare glsl >= 1.30 and GL >=
                    # 4.0
                    if self.shader_version < 1.4:
                        self.api = 'compat'
                    else:
                        self.api = 'core'
                break

        # Select the correct binary to run the test, but be as conservative as
        # possible by always selecting the lowest version that meets the
        # criteria.
        if self.api == 'gles2':
            if self.__op in ['<', '<='] or (
                    self.__op in ['=', '>='] and self.api_version is not None
                    and self.api_version < 3):
                self.prog = 'shader_runner_gles2'
            else:
                self.prog = 'shader_runner_gles3'
        else:
            self.prog = 'shader_runner'


class ShaderTest(FastSkipMixin, PiglitBaseTest):
    """ Parse a shader test file and return a PiglitTest instance

    This function parses a shader test to determine if it's a GL, GLES2 or
    GLES3 test, and then returns a PiglitTest setup properly.

    """

    def __init__(self, command, api=None, extensions=set(),
                  shader_version=None, api_version=None, env=None, **kwargs):
        super(ShaderTest, self).__init__(
            command,
            run_concurrent=True,
            api=api,
            extensions=extensions,
            shader_version=shader_version,
            api_version=api_version,
            env=env)

    @classmethod
    def new(cls, filename, installed_name=None):
        """Parse an XML file and create a new instance.

        :param str filename: The name of the file to parse
        :param str installed_name: The relative path to the file when installed
            if not the same as the parsed name
        """
        parser = Parser(filename)
        parser.parse()

        return cls(
            [parser.prog, installed_name or filename],
            run_concurrent=True,
            api=parser.api,
            extensions=parser.extensions,
            shader_version=parser.shader_version,
            api_version=parser.api_version)

    @PiglitBaseTest.command.getter
    def command(self):
        """ Add -auto, -fbo and -glsl (if needed) to the test command """

        command = super(ShaderTest, self).command
        shaderfile = os.path.join(ROOT_DIR, self._command[1])

        if options.OPTIONS.force_glsl:
            return self.keys() + [command[0]] + [shaderfile, '-auto', '-fbo', '-glsl']
        else:
            return self.keys() + [command[0]] + [shaderfile, '-auto', '-fbo']

    @command.setter
    def command(self, new):
        self._command = [n for n in new if n not in ['-auto', '-fbo']]


class MultiShaderTest(ReducedProcessMixin, PiglitBaseTest):
    """A Shader class that can run more than one test at a time.

    This class can call shader_runner with multiple shader_files at a time, and
    interpret the results, as well as handle pre-mature exit through crashes or
    from breaking import assupmtions in the utils about skipping.

    Arguments:
    filenames -- a list of absolute paths to shader test files
    """

    def __init__(self, prog, files, subtests, skips, env=None):
        super(MultiShaderTest, self).__init__(
            [prog] + files,
            subtests=subtests,
            run_concurrent=True,
            env=env)

        self.prog = prog
        self.files = files
        self.subtests = subtests
        self.skips = [FastSkip(**s) for s in skips]

    @classmethod
    def new(cls, filenames, installednames=None):
        # TODO
        assert filenames
        prog = None
        subtests = []
        skips = []

        # Walk each subtest, and either add it to the list of tests to run, or
        # determine it is skip, and set the result of that test in the subtests
        # dictionary to skip without adding it to the list of tests to run.
        for each in filenames:
            parser = Parser(each)
            parser.parse()
            subtests.append(os.path.basename(os.path.splitext(each)[0]).lower())

            if prog is not None:
                # This allows mixing GLES2 and GLES3 shader test files
                # together. Since GLES2 profiles can be promoted to GLES3, this
                # is fine.
                if parser.prog != prog:
                    # Pylint can't figure out that prog is not None.
                    if 'gles' in parser.prog and 'gles' in prog:  # pylint: disable=unsupported-membership-test
                        prog = max(parser.prog, prog)
                    else:
                        # The only way we can get here is if one is GLES and
                        # one is not, since there is only one desktop runner
                        # thus it will never fail the is parser.prog != prog
                        # check
                        raise exceptions.PiglitInternalError(
                            'GLES and GL shaders in the same command!\n'
                            'Cannot pick a shader_runner binary!\n'
                            'in file: {}'.format(os.path.dirname(each)))
            else:
                prog = parser.prog

            skips.append({
                'extensions': parser.extensions,
                'api_version': parser.api_version,
                'shader_version': parser.shader_version,
                'api': parser.api,
            })

        return cls(prog, installednames or filenames, subtests, skips)

    def _process_skips(self):
        r_files = []
        r_subtests = []
        r_skips = []
        for f, s, k in zip(self.files, self.subtests, self.skips):
            try:
                k.test()
            except TestIsSkip:
                r_skips.append(s)
            else:
                r_files.append(f)
                r_subtests.append(s)

        assert len(r_subtests) + len(r_skips) == len(self.files), \
            'not all tests accounted for'

        for name in r_skips:
            self.result.subtests[name] = status.SKIP

        self._expected = r_subtests
        self._command = [self._command[0]] + r_files

    def run(self):
        self._process_skips()
        super(MultiShaderTest, self).run()

    @PiglitBaseTest.command.getter
    def command(self):
        command = super(MultiShaderTest, self).command
        shaderfiles = (x for x in command[1:] if not x.startswith('-'))
        shaderfiles = [os.path.join(ROOT_DIR, s) for s in shaderfiles]
        return self.keys() + [command[0]] + shaderfiles + ['-auto', '-report-subtests']

    def _is_subtest(self, line):
        return line.startswith('PIGLIT TEST:')

    def _resume(self, current):
        command = [self.command[0]]
        command.extend(self.command[current + 1:])
        return command

    def _stop_status(self):
        # If the lower level framework skips then return a status for that
        # subtest as skip, and resume.
        if self.result.out.endswith('PIGLIT: {"result": "skip" }\n'):
            return status.SKIP
        if self.result.returncode > 0:
            return status.FAIL
        return status.CRASH

    def _is_cherry(self):
        # Due to the way that piglt is architected if a particular feature
        # isn't supported it causes the test to exit with status 0. There is no
        # straightforward way to fix this, so we work around it by looking for
        # the message that feature provides and marking the test as not
        # "cherry" when it is found at the *end* of stdout. (We don't want to
        # match other places or we'll end up in an infinite loop)
        return (
            self.result.returncode == 0 and not
            self.result.out.endswith(
                'not supported on this implementation\n') and not
            self.result.out.endswith(
                'PIGLIT: {"result": "skip" }\n'))
