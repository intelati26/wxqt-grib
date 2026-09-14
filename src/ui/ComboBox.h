// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef COMBOBOX_H
#define COMBOBOX_H

#include <functional>
#include <string>
#include <vector>
#include <QComboBox>
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::function;
using std::string;
using std::vector;

class ComboBox : public Widget2 {
public:
    ComboBox(Window *, const vector<string>& = {});
    int getIndex() const;
    void setIndex(size_t);
    void setIndexByPref(const string&, int);
    void setIndexByValue(const string&);
    void setList(const vector<string>&);
    vector<string> getItems() const;
    void block();
    void unblock();
    void connect(const function<void()>&);
    void setVisible(bool);
    string getValue() const;
    QComboBox * getView();

private:
    void blockSignals(bool);
    void addItemsQt();
    QComboBox * comboBox;
    Window * parent;
    vector<string> items;
};

#endif  // COMBOBOX_H
