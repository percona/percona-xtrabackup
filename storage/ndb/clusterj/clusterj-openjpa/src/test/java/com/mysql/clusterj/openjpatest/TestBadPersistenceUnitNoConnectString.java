/*
 *  Copyright 2010 Sun Microsystems, Inc.
 *   All rights reserved. Use is subject to license terms.
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

package com.mysql.clusterj.openjpatest;

import javax.persistence.EntityManagerFactory;
import javax.persistence.Persistence;

import junit.framework.TestCase;

/**
 *
 */
public class TestBadPersistenceUnitNoConnectString extends TestCase {

    protected String getPersistenceUnitName() {
        return "BadPUNoConnectString";
    }
    public void test() {
        try {
            EntityManagerFactory emf = Persistence.createEntityManagerFactory(
                    getPersistenceUnitName());
            emf.createEntityManager();
            if(emf != null) {
                fail("Unexpected emf for null connectString");
            }
            
        } catch (RuntimeException ex) {
            // see if it has the connectString message
            if (!(ex.getMessage().contains("connectString"))) {
                fail("Wrong exception thrown. " +
                        "Expected Exception with connectString; got " +
                        ex.getMessage());
            }
        }
    }

}
