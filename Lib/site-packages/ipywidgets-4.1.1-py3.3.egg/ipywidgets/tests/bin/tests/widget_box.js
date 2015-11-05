/// <reference path="../notebook_test.d.ts" />
var base = require('../base');
// Globals
var widget_box_selector = '.widget-area .widget-subarea .widget-box';
var widget_box_button_selector = '.widget-area .widget-subarea .widget-box button';
var widget_button_selector = '.widget-area .widget-subarea button';
// Test container class
base.tester
    .start_notebook_then()
    .cell("\n    import ipywidgets as widgets \n    from IPython.display import display, clear_output\n    container = widgets.Box()\n    button = widgets.Button()\n    container.children = [button]\n    display(container)\n    container._dom_classes = [\"my-test-class\"]\n    print(\"Success\")\n    ", function (container_index) {
    this.test.assertEquals(this.notebook.get_output(container_index).text, 'Success\n', 'Create container cell executed with correct output.');
    // Wait for the widgets to actually display.
    this
        .wait_for_element(container_index, widget_box_selector)
        .wait_for_element(container_index, widget_box_button_selector)
        .then(function () {
        this.test.assert(this.notebook.cell_element_exists(container_index, '.widget-area .widget-subarea'), 'Widget subarea exists.');
        this.test.assert(this.notebook.cell_element_exists(container_index, widget_box_selector), 'Widget container exists.');
        this.test.assert(this.notebook.cell_element_exists(container_index, '.widget-area .widget-subarea .my-test-class'), '_dom_classes works.');
        this.test.assert(this.notebook.cell_element_exists(container_index, widget_box_button_selector), 'Container parent/child relationship works.');
    })
        .cell("\n            container.box_style = \"success\"\n            print(\"Success\")\n            ", function (index) {
        this.test.assertEquals(this.notebook.get_output(index).text, 'Success\n', 'Set box_style cell executed with correct output.');
        this.test.assert(this.notebook.cell_element_exists(container_index, '.widget-box.alert-success'), 'Set box_style works.');
    })
        .cell("\n            container._dom_classes = []\n            print(\"Success\")\n            ", function (index) {
        this.test.assertEquals(this.notebook.get_output(index).text, 'Success\n', 'Remove container class cell executed with correct output.');
        this.test.assert(!this.notebook.cell_element_exists(container_index, '.widget-area .widget-subarea .my-test-class'), '_dom_classes can be used to remove a class.');
    });
})
    .cell("\n    display(button)\n    print(\"Success\")\n    ", function (boxalone_index) {
    this.test.assertEquals(this.notebook.get_output(boxalone_index).text, 'Success\n', 'Display container child executed with correct output.');
    // Wait for the widget to actually display.
    this
        .wait_for_element(boxalone_index, widget_button_selector)
        .then(function () {
        this.test.assert(!this.notebook.cell_element_exists(boxalone_index, widget_box_selector), 'Parent container not displayed.');
        this.test.assert(this.notebook.cell_element_exists(boxalone_index, widget_button_selector), 'Child displayed.');
    });
})
    .stop_notebook_then();
