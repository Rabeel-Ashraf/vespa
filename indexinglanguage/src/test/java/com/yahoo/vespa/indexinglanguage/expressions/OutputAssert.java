// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.indexinglanguage.expressions;

import com.yahoo.document.DataType;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

/**
 * @author Simon Thoresen Hult
 */
class OutputAssert {

    public static void assertVerify(OutputExpression exp) {
        assertVerify(new MyAdapter(null), DataType.INT, exp);
        assertVerify(new MyAdapter(null), DataType.STRING, exp);
        assertVerifyThrows(new MyAdapter(null), null, exp, "Invalid expression '" + exp + "': Expected input, but no input is specified");
        assertVerifyThrows(new MyAdapter(new VerificationException((Expression) null, "foo")), DataType.INT, exp, "Invalid expression 'null': foo");
    }

    public static void assertVerify(FieldTypeAdapter adapter, DataType value, Expression exp) {
        var context = new VerificationContext(adapter);
        assertEquals(value, exp.setInputType(value, context));
    }

    public static void assertVerifyThrows(FieldTypeAdapter adapter, DataType value, Expression exp,
                                          String expectedException)
    {
        try {
            new VerificationContext(adapter).verify(exp);
            fail();
        } catch (VerificationException e) {
            assertEquals(expectedException, e.getMessage());
        }
    }

    private static class MyAdapter implements FieldTypeAdapter {

        final RuntimeException e;

        MyAdapter(RuntimeException e) {
            this.e = e;
        }

        @Override
        public DataType getInputType(Expression exp, String fieldName) {
            throw new AssertionError();
        }

        @Override
        public void tryOutputType(Expression exp, String fieldName, DataType valueType) {
            if (e != null) {
                throw e;
            }
        }
    }

}
