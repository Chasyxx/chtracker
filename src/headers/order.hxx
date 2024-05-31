/*
  Copyright (C) 2024 Chase Taylor
  
  This file is part of chTRACKER at path ./src/headers/order.hxx
  This is a declaration file; For implementation see path
  ./src/order/order.cxx

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

#ifndef _CHTRACKER_ORDER_HXX
#define _CHTRACKER_ORDER_HXX
#include <vector>

enum class effectTypes {
    null,
    arpeggio,
    pitchUp,
    pitchDown,
    volumeUp,
    volumeDown,
    instrumentVariation
};

struct effect {
    effectTypes type = effectTypes::null;
    unsigned short effect;
};

struct effectValues {
    unsigned char a;
    unsigned char b;
    unsigned char c;
    unsigned char d;
};

extern struct effectValues effectTo2Values(effect e);
extern struct effectValues effectTo4Values(effect e);

enum class rowFeature {
    empty,
    note,
    note_cut
};

struct row {
    rowFeature feature = rowFeature::empty;
    char note = 'A';
    char octave = 4;
    unsigned char volume = 240;
    std::vector<effect> effects = std::vector<effect>(4);
};

class order {
    private:
    std::vector<row> rows;
    public:
    order(unsigned short size);

    void setRowCount(unsigned short size);
    unsigned short rowCount();
    row* at(unsigned short idx);
};

class instrumentOrderTable {
    private:
    std::vector<order> orders;
    std::vector<order> erase_orders;
    unsigned short rowCount = 32;
    public:
    instrumentOrderTable(unsigned short size);
    unsigned char add_order();
    unsigned char order_count();
    void remove_order(unsigned char idx);
    void set_row_count(unsigned short size);
    order* at(unsigned char idx);
};

class orderStorage {
    private:
    std::vector<instrumentOrderTable> orderTables;
    std::vector<instrumentOrderTable> erase_orderTables;
    unsigned short row_count = 32;
    public:
    orderStorage(unsigned short size);
    unsigned char addTable();
    unsigned char tableCount();
    void removeTable(unsigned char idx);
    void setRowCount(unsigned short size);
    unsigned short rowCount();
    instrumentOrderTable* at(unsigned char idx);
};

class orderIndexRow {
    private:
        std::vector<unsigned char> indexes;
        std::vector<unsigned char> erase_indexes;
    public:
        unsigned char addInst();
        unsigned char instCount();
        void removeInst(unsigned char idx);
        unsigned char at(unsigned char idx);
        void set(unsigned char idx, unsigned char value);
        void increment(unsigned char idx);
        void decrement(unsigned char idx);
        unsigned char size();
};

class orderIndexStorage {
    private:
    std::vector<orderIndexRow> rows;
    std::vector<orderIndexRow> erase_rows;
    public:
    unsigned char addInst();
    unsigned char instCount(unsigned char fallback);
    void removeInst(unsigned char idx);
    unsigned short addRow();
    unsigned short rowCount();
    void removeRow(unsigned short idx);
    orderIndexRow* at(unsigned short idx);
};
#endif
