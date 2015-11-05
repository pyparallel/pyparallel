// Copyright (c) Jupyter Development Team.
// Distributed under the terms of the Modified BSD License.
/**
 * Represents a Notebook used for testing.
 */
var Notebook = (function () {
    function Notebook(tester) {
        this._cell_index = 0;
        this._cells = [];
        this._cell_outputs = [];
        this._cell_outputs_errors = [];
        this._tester = tester;
        this._open_new_notebook();
    }
    Object.defineProperty(Notebook.prototype, "cell_index", {
        /**
         * Index of the last appended cell.
         */
        get: function () {
            return this._cell_index;
        },
        enumerable: true,
        configurable: true
    });
    /**
     * Is the notebook busy
     */
    Notebook.prototype.is_busy = function () {
        return this._tester.evaluate(function () {
            return IPython._status === 'busy';
        });
    };
    /**
     * Is the notebook idle
     */
    Notebook.prototype.is_idle = function () {
        return this._tester.evaluate(function () {
            return IPython._status === 'idle';
        });
    };
    /**
     * Does a cell have output
     */
    Notebook.prototype.has_output = function (cell_index, output_index) {
        if (output_index === void 0) { output_index = 0; }
        return this._tester.evaluate(function get_output(c, o) {
            var cell = IPython.notebook.get_cell(c);
            return cell.output_area.outputs.length > o;
        }, { c: cell_index, o: output_index });
    };
    /**
     * Get the output of a cell
     */
    Notebook.prototype.get_output = function (cell_index, output_index) {
        if (output_index === void 0) { output_index = 0; }
        return this._tester.evaluate(function get_output(c, o) {
            var cell = IPython.notebook.get_cell(c);
            if (cell.output_area.outputs.length > o) {
                return cell.output_area.outputs[o];
            }
            else {
                return undefined;
            }
        }, { c: cell_index, o: output_index });
    };
    /**
     * Get the cell execution cached outputs.
     */
    Notebook.prototype.get_cached_outputs = function (cell_index) {
        return this._cell_outputs[cell_index];
    };
    /**
     * Get the cell execution cached output errors.
     */
    Notebook.prototype.get_cached_output_errors = function (cell_index) {
        return this._cell_outputs_errors[cell_index];
    };
    /**
     * Check if an element exists in a cell.
     */
    Notebook.prototype.cell_element_exists = function (cell_index, selector) {
        return this._tester.evaluate(function (c, s) {
            var $cell = IPython.notebook.get_cell(c).element;
            return $cell.find(s).length > 0;
        }, cell_index, selector);
    };
    /**
     * Utility function that allows us to execute a jQuery function
     * on an element within a cell.
     */
    Notebook.prototype.cell_element_function = function (cell_index, selector, function_name, function_args) {
        return this._tester.evaluate(function (c, s, f, a) {
            var $cell = IPython.notebook.get_cell(c).element;
            var $el = $cell.find(s);
            return $el[f].apply($el, a);
        }, cell_index, selector, function_name, function_args);
    };
    /**
     * Get the URL for the notebook server.
     */
    Notebook.prototype.get_notebook_server = function () {
        // Get the URL of a notebook server on which to run tests.
        var port = this._tester.cli.get("port");
        port = (typeof port === 'undefined') ? '8888' : port;
        return this._tester.cli.get("url") || ('http://127.0.0.1:' + port);
    };
    /**
     * Append a cell to the notebook
     * @return cell index
     */
    Notebook.prototype.append_cell = function (contents, cell_type) {
        // Inserts a cell at the bottom of the notebook
        // Returns the new cell's index.
        var index = this._tester.evaluate(function (t, c) {
            var cell = IPython.notebook.insert_cell_at_bottom(t);
            cell.set_text(c);
            return IPython.notebook.find_cell_index(cell);
        }, cell_type, contents);
        // Increment the logged cell index.
        this._cell_index++;
        this._cells.push(contents);
        return index;
    };
    /**
     * Get an appended cell's contents.
     * @return contents
     */
    Notebook.prototype.get_cell = function (index) {
        return this._cells[index];
    };
    /**
     * Execute a cell
     * @param index
     * @param expect_error - expect an error to occur when running the cell
     */
    Notebook.prototype.execute_cell = function (index, expect_error) {
        var _this = this;
        if (expect_error === void 0) { expect_error = false; }
        this._tester.then(function () {
            _this._tester.evaluate(function (index) {
                var cell = IPython.notebook.get_cell(index);
                cell.execute();
            }, index);
        });
        this._tester.wait_for_idle();
        // Check for errors.
        this._tester.then(function () {
            var nonerrors = _this._tester.evaluate(function (index) {
                var cell = IPython.notebook.get_cell(index);
                var outputs = cell.output_area.outputs;
                var nonerrors = [];
                for (var i = 0; i < outputs.length; i++) {
                    if (outputs[i].output_type !== 'error') {
                        nonerrors.push(outputs[i]);
                    }
                }
                return nonerrors;
            }, index);
            _this._cell_outputs.push(nonerrors);
            var errors = _this._tester.evaluate(function (index) {
                var cell = IPython.notebook.get_cell(index);
                var outputs = cell.output_area.outputs;
                var errors = [];
                for (var i = 0; i < outputs.length; i++) {
                    if (outputs[i].output_type === 'error') {
                        errors.push(outputs[i]);
                    }
                }
                return errors;
            }, index);
            _this._cell_outputs_errors.push(errors);
            var error = _this._tester.evaluate(function (index) {
                var cell = IPython.notebook.get_cell(index);
                var outputs = cell.output_area.outputs;
                for (var i = 0; i < outputs.length; i++) {
                    if (outputs[i].output_type == 'error') {
                        return outputs[i];
                    }
                }
                return false;
            }, index);
            if (error === null) {
                _this._tester.test.fail("Failed to check for error output");
            }
            if (!expect_error && error !== false) {
                _this._tester.test.fail("Error running cell");
            }
            if (expect_error && error === false) {
                _this._tester.test.fail("An error was expected but the cell didn't raise one");
            }
        });
    };
    /**
     * Opens a new notebook.
     */
    Notebook.prototype._open_new_notebook = function () {
        var _this = this;
        // Create and open a new notebook.
        var baseUrl = this.get_notebook_server();
        this._tester.start(baseUrl);
        this._tester.waitFor(function () { return _this._page_loaded(); });
        this._tester.waitForSelector('#kernel-python2 a, #kernel-python3 a');
        this._tester.thenClick('#kernel-python2 a, #kernel-python3 a');
        this._tester.waitForPopup('');
        this._tester.withPopup('', function () { this.waitForSelector('.CodeMirror-code'); });
        this._tester.then(function () {
            this.open(this.popups[0].url);
        });
        this._tester.waitFor(function () { return _this._page_loaded(); });
        // Make sure the kernel has started
        this._tester.waitFor(function () { return _this._kernel_running(); });
        // track the IPython busy/idle state
        this._tester.thenEvaluate(function () {
            require(['base/js/namespace', 'base/js/events'], function (IPython, events) {
                events.on('kernel_idle.Kernel', function () {
                    IPython._status = 'idle';
                });
                events.on('kernel_busy.Kernel', function () {
                    IPython._status = 'busy';
                });
            });
        });
        // Because of the asynchronous nature of SlimerJS (Gecko), we need to make
        // sure the notebook has actually been loaded into the IPython namespace
        // before running any tests.
        this._tester.waitFor(function () {
            return this.evaluate(function () {
                return IPython.notebook;
            });
        });
    };
    /**
     * Whether or not the page has loaded.
     */
    Notebook.prototype._page_loaded = function () {
        // Return whether or not the kernel is running.
        return this._tester.evaluate(function () {
            return typeof IPython !== "undefined" &&
                IPython.page !== undefined;
        });
    };
    /**
     * Whether or not the kernel is running
     */
    Notebook.prototype._kernel_running = function () {
        // Return whether or not the kernel is running.
        return this._tester.evaluate(function () {
            return IPython &&
                IPython.notebook &&
                IPython.notebook.kernel &&
                IPython.notebook.kernel.is_connected();
        });
    };
    return Notebook;
})();
exports.Notebook = Notebook;
