/// <reference path="../notebook_test.d.ts" />
var base = require('../base');
base.tester
    .start_notebook_then()
    .cell("from ipywidgets import Widget")
    .then(function () {
    // Check if the WidgetManager class is defined.
    this.test.assert(this.evaluate(function () {
        return IPython.WidgetManager !== undefined;
    }), 'WidgetManager class is defined');
    // Check if the widget manager has been instantiated.
    this.test.assert(this.evaluate(function () {
        return IPython.notebook.kernel.widget_manager !== undefined;
    }), 'Notebook widget manager instantiated');
    // Try creating a widget from Javascript.
    this.evaluate(function () {
        IPython.notebook.kernel.widget_manager.new_widget({
            model_name: 'WidgetModel',
            widget_class: 'ipywidgets.IntSlider' })
            .then(function (model) {
            console.log('Create success!', model);
            window.slider_id = model.id;
        }, function (error) { console.log(error); });
    });
})
    .waitFor(function () {
    return this.evaluate(function () {
        return window.slider_id !== undefined;
    });
})
    .cell("\n    widget = list(Widget.widgets.values())[0]\n    print(widget.model_id)\n    ", function (index) {
    var output = this.notebook.get_output(index).text.trim();
    var slider_id = this.evaluate(function () { return window.slider_id; });
    this.test.assertEquals(output, slider_id, "Widget created from the front-end.");
})
    .cell("\n    from ipywidgets import HTML\n    from IPython.display import display\n    display(HTML(value=\"<div id='hello'></div>\"))\n    ")
    .cell("\n    display(HTML(value=\"<div id='world'></div>\"))\n    ")
    .waitForSelector('#hello')
    .waitForSelector('#world')
    .then(function () {
    this.test.assertExists('#hello', 'Hello HTML widget constructed.');
    this.test.assertExists('#world', 'World HTML widget constructed.');
    // Save the notebook.
    this.evaluate(function () {
        IPython.notebook.save_notebook(false).then(function () {
            window.was_saved = true;
        });
    });
})
    .waitFor(function () {
    return this.evaluate(function () {
        return window.was_saved;
    });
}, function () {
    var _this = this;
    this
        .reload()
        .waitForSelector('#hello')
        .waitForSelector('#world', function () {
        _this.test.assertExists('#hello', 'Hello HTML widget persisted.');
        _this.test.assertExists('#world', 'World HTML widget persisted.');
    });
})
    .stop_notebook_then();
