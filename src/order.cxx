/*
  Copyright (C) 2024 Chase Taylor

  This file is part of chTRACKER at path ./src/order/order.cxx

  chTRACKER is free software: you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version.

  chTRACKER is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
  A PARTICULAR PURPOSE. See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with
  chTRACKER. If not, see <https://www.gnu.org/licenses/>.

  Chase Taylor @ creset200@gmail.com
*/

#include <stdexcept>
#include "order.hxx"

// effect
struct effectValues effectTo2Values(effect e) {
    unsigned short v = e.effect;
    unsigned char a = v>>8&255;
    unsigned char b = v&255;
    return {a,b,0,0};
}

struct effectValues effectTo4Values(effect e) {
    unsigned short v = e.effect;
    unsigned char a = v>>12&15;
    unsigned char b = v>>8&15;
    unsigned char c = v>>4&15;
    unsigned char d = v&15;
    return {a,b,c,d};
}

// row


// order
order::order(unsigned short size): rows(size) {}

void order::setRowCount(unsigned short size) {
    rows.resize(size);
}

unsigned short order::rowCount() const {
    return rows.size();
}

row* order::at(unsigned short idx) {
    return &rows.at(idx);
}


// instrumentOrderTable
instrumentOrderTable::instrumentOrderTable(unsigned short size): rowCount(size) {};
unsigned char instrumentOrderTable::add_order() {
    orders.push_back(order(rowCount));
    return orders.size()-1;
}
unsigned char instrumentOrderTable::order_count() const {
    return orders.size();
}
void instrumentOrderTable::remove_order(unsigned char idx) {
    if(idx>=order_count()) throw std::out_of_range("instrumentOrderTable::remove_order");

    for(unsigned char i = order_count()-1; i>idx; i--) {
        order o = orders.back();
        orders.pop_back();
        erase_orders.push_back(o);
    }
    orders.pop_back();
    while(!erase_orders.empty()) {
        order o = erase_orders.back();
        erase_orders.pop_back();
        orders.push_back(o);
    }
}
void instrumentOrderTable::set_row_count(unsigned short size) {
    rowCount = size;
    for(unsigned char i = 0; i < orders.size(); i++) {
        orders.at(i).setRowCount(size);
    }
}
order* instrumentOrderTable::at(unsigned char idx) {
    return &orders.at(idx);
}


// orderStorage
orderStorage::orderStorage(unsigned short size): row_count(size) {}
unsigned char orderStorage::addTable() {
    orderTables.push_back(instrumentOrderTable(row_count));
    return orderTables.size()-1;
}
unsigned char orderStorage::tableCount() const {
    return orderTables.size();
}
void orderStorage::removeTable(unsigned char idx) {
    if(idx>=tableCount()) throw std::out_of_range("orderStorage::remove_table");

    for(unsigned char i = tableCount()-1; i>idx; i--) {
        instrumentOrderTable table = orderTables.back();
        orderTables.pop_back();
        erase_orderTables.push_back(table);
    }
    orderTables.pop_back();
    while(!erase_orderTables.empty()) {
        instrumentOrderTable table = erase_orderTables.back();
        erase_orderTables.pop_back();
        orderTables.push_back(table);
    }
}
void orderStorage::setRowCount(unsigned short size) {
    row_count = size;
    for(unsigned long i = 0; i < orderTables.size(); i++) {
        orderTables.at(i).set_row_count(size);
    }
}
unsigned short orderStorage::rowCount() const {
    return row_count;
}
instrumentOrderTable* orderStorage::at(unsigned char idx) {
    return &orderTables.at(idx);
}

// orderIndexRow

unsigned char orderIndexRow::addInst() {
    indexes.push_back(0);
    return indexes.size()-1;
}
unsigned char orderIndexRow::instCount() const {
    return indexes.size();
}
void orderIndexRow::removeInst(unsigned char idx) {
    if(idx>=instCount()) throw std::out_of_range("orderIndexRow::removeInst");

    for(unsigned char i = instCount()-1; i>idx; i--) {
        unsigned char index = indexes.back();
        indexes.pop_back();
        erase_indexes.push_back(index);
    }
    indexes.pop_back();
    while(!erase_indexes.empty()) {
        unsigned char index = erase_indexes.back();
        erase_indexes.pop_back();
        indexes.push_back(index);
    }
}
unsigned char orderIndexRow::at(unsigned char idx) {
    return indexes.at(idx);
}
void orderIndexRow::set(unsigned char idx, unsigned char value) {
    if(idx >= instCount()) throw std::out_of_range("orderIndexRow::set");
    indexes[idx] = value;
}
void orderIndexRow::increment(unsigned char idx) {
    if(idx >= instCount()) throw std::out_of_range("orderIndexRow::increment");
    indexes[idx]++;
}
void orderIndexRow::decrement(unsigned char idx) {
    if(idx >= instCount()) throw std::out_of_range("orderIndexRow::decrement");
    indexes[idx]--;
}
// orderIndexStorage

unsigned char orderIndexStorage::addInst() {
    unsigned char temp = 255;
    for(unsigned short i = 0; i < rows.size(); i++) {
        temp = rows.at(i).addInst();
    }
    return temp;
}

unsigned char orderIndexStorage::instCount(unsigned char fallback) const {
    if(rows.size()==0) return fallback;
    try {
        return rows.at(0).instCount();
    } catch (std::out_of_range&) {
        return fallback;
    }
}

void orderIndexStorage::removeInst(unsigned char idx) {
    for(unsigned short i = 0; i < rowCount(); i++) {
        rows.at(i).removeInst(idx);
    }
}

unsigned short orderIndexStorage::addRow() {
    orderIndexRow row;
    for(unsigned char i=0; i<instCount(0); i++) row.addInst();
    rows.push_back(row);
    return rows.size()-1;
}

unsigned short orderIndexStorage::rowCount() const {
    return rows.size();
}

void orderIndexStorage::removeRow(unsigned short idx) {
    if(idx>=rowCount()) throw std::out_of_range("orderIndexStorage::removeRow");

    for(unsigned char i = rowCount()-1; i>idx; i--) {
        orderIndexRow row = rows.back();
        rows.pop_back();
        erase_rows.push_back(row);
    }
    rows.pop_back();
    while(!erase_rows.empty()) {
        orderIndexRow row = erase_rows.back();
        erase_rows.pop_back();
        rows.push_back(row);
    }
}

orderIndexRow* orderIndexStorage::at(unsigned short idx) {
    return &rows.at(idx);
}
