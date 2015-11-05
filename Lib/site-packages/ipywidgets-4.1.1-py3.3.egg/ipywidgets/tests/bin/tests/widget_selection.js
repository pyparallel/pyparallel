/// <reference path="../notebook_test.d.ts" />
var base = require('../base');
// Globals
var combo_selector = '.widget-area .widget-subarea .widget-hbox .btn-group .widget-combo-btn';
var multibtn_selector = '.widget-area .widget-subarea .widget-hbox.widget-toggle-buttons .btn-group';
var radio_selector = '.widget-area .widget-subarea .widget-hbox .widget-radio-box';
var list_selector = '.widget-area .widget-subarea .widget-hbox .widget-listbox';
var selection_values = 'abcd';
var check_state = function (selection_index, context, index, state) {
    if (0 <= index && index < selection_values.length) {
        var multibtn_state = context.notebook.cell_element_function(selection_index, multibtn_selector + ' .btn:nth-child(' + (index + 1) + ')', 'hasClass', ['active']);
        var radio_state = context.notebook.cell_element_function(selection_index, radio_selector + ' .radio:nth-child(' + (index + 1) + ') input', 'prop', ['checked']);
        var list_val = context.notebook.cell_element_function(selection_index, list_selector, 'val');
        var combo_val = context.notebook.cell_element_function(selection_index, combo_selector, 'html');
        var val = selection_values.charAt(index);
        var list_state = (val == list_val);
        var combo_state = (val == combo_val);
        return multibtn_state == state &&
            radio_state == state &&
            list_state == state &&
            combo_state == state;
    }
    return true;
};
var verify_selection = function (selection_index, context, index) {
    for (var i = 0; i < selection_values.length; i++) {
        if (!check_state(selection_index, context, i, i == index)) {
            return false;
        }
    }
    return true;
};
// Test selection class
base.tester
    .start_notebook_then()
    .cell("\n    import ipywidgets as widgets\n    from IPython.display import display, clear_output\n    print(\"Success\")\n    ")
    .cell("\n    options=[\"" + selection_values + "\"[i] for i in range(4)]\n    options.append('  spaces  ')\n    selection = [widgets.Dropdown(options=options),\n        widgets.ToggleButtons(options=options),\n        widgets.RadioButtons(options=options),\n        widgets.Select(options=options)]\n    [display(selection[i]) for i in range(4)]\n    for widget in selection:\n        def handle_change(name,old,new):\n            for other_widget in selection:\n                other_widget.value = new\n        widget.on_trait_change(handle_change, \"value\")\n    print(\"Success\")\n    ", function (selection_index) {
    this.test.assertEquals(this.notebook.get_output(selection_index).text, 'Success\n', 'Create selection cell executed with correct output.');
    // Wait for the widgets to actually display.
    this
        .wait_for_element(selection_index, combo_selector)
        .wait_for_element(selection_index, multibtn_selector)
        .wait_for_element(selection_index, radio_selector)
        .wait_for_element(selection_index, list_selector)
        .then(function () {
        this.test.assert(this.notebook.cell_element_exists(selection_index, '.widget-area .widget-subarea'), 'Widget subarea exists.');
        this.test.assert(this.notebook.cell_element_exists(selection_index, combo_selector), 'Widget combobox exists.');
        this.test.assert(this.notebook.cell_element_exists(selection_index, multibtn_selector), 'Widget multibutton exists.');
        this.test.assert(this.notebook.cell_element_exists(selection_index, radio_selector), 'Widget radio buttons exists.');
        this.test.assert(this.notebook.cell_element_exists(selection_index, list_selector), 'Widget list exists.');
        // Verify that no items are selected.
        this.test.assert(verify_selection(selection_index, this, 0), 'Default first item selected.');
    })
        .cell("\n            for widget in selection:\n                widget.value = \"a\"\n            print(\"Success\")\n            ", function (index) {
        this.test.assertEquals(this.notebook.get_output(index).text, 'Success\n', 'Python select item executed with correct output.');
        // Verify that the first item is selected.
        this.test.assert(verify_selection(selection_index, this, 0), 'Python selected');
        // Verify that selecting a radio button updates all of the others.
        this.notebook.cell_element_function(selection_index, radio_selector + ' .radio:nth-child(2) input', 'click');
    })
        .wait_for_idle()
        .then(function () {
        this.test.assert(verify_selection(selection_index, this, 1), 'Radio button selection updated view states correctly.');
        // Verify that selecting a list option updates all of the others.
        this.notebook.cell_element_function(selection_index, list_selector + ' option:nth-child(3)', 'click');
    })
        .wait_for_idle()
        .then(function () {
        this.test.assert(verify_selection(selection_index, this, 2), 'List selection updated view states correctly.');
        // Verify that selecting the option with spaces works
        this.notebook.cell_element_function(selection_index, list_selector + ' option:nth-child(5)', 'click');
    })
        .wait_for_idle()
        .then(function () {
        this.test.assert(verify_selection(selection_index, this, 4), 'List selection of space element updated view states correctly.');
        // Verify that selecting a multibutton option updates all of the others.
        // Bootstrap3 has changed the toggle button group behavior.  Two clicks
        // are required to actually select an item.
        this.notebook.cell_element_function(selection_index, multibtn_selector + ' .btn:nth-child(4)', 'click');
        this.notebook.cell_element_function(selection_index, multibtn_selector + ' .btn:nth-child(4)', 'click');
    })
        .wait_for_idle()
        .then(function () {
        this.test.assert(verify_selection(selection_index, this, 3), 'Multibutton selection updated view states correctly.');
        // Verify that selecting a combobox option updates all of the others.
        this.notebook.cell_element_function(selection_index, '.widget-area .widget-subarea .widget-hbox .btn-group ul.dropdown-menu li:nth-child(3) a', 'click');
    })
        .wait_for_idle()
        .then(function () {
        this.test.assert(verify_selection(selection_index, this, 2), 'Combobox selection updated view states correctly.');
    })
        .wait_for_idle()
        .cell("\n            from copy import copy\n            for widget in selection:\n                d = copy(widget.options)\n                d.append(\"z\")\n                widget.options = d\n            selection[0].value = \"z\"\n            ", function (index) {
        // Verify that selecting a combobox option updates all of the others.
        this.test.assert(verify_selection(selection_index, this, 4), 'Item added to selection widget.');
    });
})
    .stop_notebook_then();
