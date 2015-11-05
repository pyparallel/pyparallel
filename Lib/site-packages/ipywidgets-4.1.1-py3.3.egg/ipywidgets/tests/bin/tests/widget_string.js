/// <reference path="../notebook_test.d.ts" />
var base = require('../base');
// Globals
var textbox_selector = '.widget-area .widget-subarea .widget-hbox input[type=text]';
var textarea_selector = '.widget-area .widget-subarea .widget-hbox textarea';
var latex_selector = '.widget-area .widget-subarea div span.MathJax_Preview';
// Test widget string class
base.tester
    .start_notebook_then()
    .cell("\n    import ipywidgets as widgets\n    from IPython.display import display, clear_output\n    string_widget = [widgets.Text(value = \"xyz\", placeholder = \"abc\"),\n        widgets.Textarea(value = \"xyz\", placeholder = \"def\"),\n        widgets.HTML(value = \"xyz\"),\n        widgets.Latex(value = \"$\\\\LaTeX{}$\")]\n    [display(widget) for widget in string_widget]\n    print(\"Success\")\n    ", function (string_index) {
    this.test.assertEquals(this.notebook.get_output(string_index).text, 'Success\n', 'Create string widget cell executed with correct output.');
    // Wait for the widget to actually display.
    this
        .wait_for_element(string_index, textbox_selector)
        .wait_for_element(string_index, textarea_selector)
        .wait_for_element(string_index, latex_selector)
        .then(function () {
        this.test.assert(this.notebook.cell_element_exists(string_index, '.widget-area .widget-subarea'), 'Widget subarea exists.');
        this.test.assert(this.notebook.cell_element_exists(string_index, textbox_selector), 'Textbox exists.');
        this.test.assert(this.notebook.cell_element_exists(string_index, textarea_selector), 'Textarea exists.');
        this.test.assert(this.notebook.cell_element_function(string_index, textarea_selector, 'val') == 'xyz', 'Python set textarea value.');
        this.test.assert(this.notebook.cell_element_function(string_index, textbox_selector, 'val') == 'xyz', 'Python set textbox value.');
        this.test.assert(this.notebook.cell_element_exists(string_index, latex_selector), 'MathJax parsed the LaTeX successfully.');
        this.test.assert(this.notebook.cell_element_function(string_index, textarea_selector, 'attr', ['placeholder']) == 'def', 'Python set textarea placeholder.');
        this.test.assert(this.notebook.cell_element_function(string_index, textbox_selector, 'attr', ['placeholder']) == 'abc', 'Python set textbox placehoder.');
    });
})
    .stop_notebook_then();
