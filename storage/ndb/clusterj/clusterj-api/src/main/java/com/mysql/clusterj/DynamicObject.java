/*
   Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

package com.mysql.clusterj;

public abstract class DynamicObject {

    private DynamicObjectDelegate delegate;

    public String table() {
        return null;
    }

    public final void delegate(DynamicObjectDelegate delegate) {
        this.delegate = delegate;
    }

    public final DynamicObjectDelegate delegate() {
        return delegate;
    }

    public final Object get(int columnNumber) {
        return delegate.get(columnNumber);
    }

    public final void set(int columnNumber, Object value) {
        delegate.set(columnNumber, value);
    }

    public final ColumnMetadata[] columnMetadata() {
        return delegate.columnMetadata();
    }

    public Boolean found() {
        return delegate.found();
    }

    protected void finalize() throws Throwable {
        try {
            if (delegate != null) {
                delegate.release();
            }
        } finally {
            super.finalize();
        }
    }
}
