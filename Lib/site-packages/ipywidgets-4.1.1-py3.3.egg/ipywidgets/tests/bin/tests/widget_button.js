/// <reference path="../notebook_test.d.ts" />
var base = require('../base');
// Globals
var widget_button_selector = '.widget-area .widget-subarea button';
// Test widget button class
base.tester
    .start_notebook_then()
    .cell("\n    import ipywidgets as widgets\n    from IPython.display import display, clear_output\n    button = widgets.Button(description=\"Title\")\n    display(button)\n    print(\"Success\")\n    def handle_click(sender):\n        display(\"Clicked\")\n    button.on_click(handle_click)\n    ", function (button_index) {
    this.test.assertEquals(this.notebook.get_output(button_index).text, 'Success\n', 'Create button cell executed with correct output.');
    // Wait for the widgets to actually display.
    this
        .wait_for_element(button_index, widget_button_selector)
        .then(function () {
        this.test.assert(this.notebook.cell_element_exists(button_index, '.widget-area .widget-subarea'), 'Widget subarea exists.');
        this.test.assert(this.notebook.cell_element_exists(button_index, widget_button_selector), 'Widget button exists.');
        this.test.assert(this.notebook.cell_element_function(button_index, widget_button_selector, 'html') == '<i class="fa"></i>Title', 'Set button description.');
        this.notebook.cell_element_function(button_index, widget_button_selector, 'click');
    })
        .wait_for_output(button_index, 1)
        .then(function () {
        this.test.assertEquals(this.notebook.get_output(button_index, 1).data['text/plain'], "'Clicked'", 'Button click event fires.');
    });
})
    .stop_notebook_then();
