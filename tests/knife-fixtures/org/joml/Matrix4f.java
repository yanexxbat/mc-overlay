package org.joml;
/** Test fixture only. Never included in the portable helper. */
public final class Matrix4f {
    public final float[] values=new float[16];
    public Matrix4f() { values[0]=values[5]=values[10]=values[15]=1; }
    public float[] get(float[] output) { System.arraycopy(values,0,output,0,16); return output; }
}
