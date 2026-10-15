package mcov;

import java.util.*;

/** Original procedural meshes and motion, inspired by the requested knife types.
 * No Anubis binaries, assets, Java sources or Minecraft resources are bundled. */
public final class KnifeGeometry {
    private KnifeGeometry() {}
    static float smooth(float t) { t=Math.max(0,Math.min(1,t)); return t*t*(3-2*t); }
    public static final class Matrix {
        public final float[] a=new float[16];
        public Matrix() { a[0]=a[5]=a[10]=a[15]=1; }
        public Matrix(float[] source) { System.arraycopy(source,0,a,0,16); }
        Matrix mul(Matrix b) {
            float[] r=new float[16];
            for(int c=0;c<4;++c) for(int row=0;row<4;++row) for(int k=0;k<4;++k) r[c*4+row]+=a[k*4+row]*b.a[c*4+k];
            System.arraycopy(r,0,a,0,16); return this;
        }
        Matrix move(float x,float y,float z) { Matrix b=new Matrix(); b.a[12]=x; b.a[13]=y; b.a[14]=z; return mul(b); }
        Matrix scale(float x,float y,float z) { Matrix b=new Matrix(); b.a[0]=x; b.a[5]=y; b.a[10]=z; return mul(b); }
        Matrix turn(float x,float y,float z) {
            for(int axis=0;axis<3;++axis) {
                float v=axis==0?x:axis==1?y:z,c=(float)Math.cos(v),s=(float)Math.sin(v); Matrix b=new Matrix();
                int i=(axis+1)%3,j=(axis+2)%3; b.a[i*4+i]=b.a[j*4+j]=c; b.a[i*4+j]=s; b.a[j*4+i]=-s; mul(b);
            }
            return this;
        }
        public float[] point(float x,float y,float z) { return new float[]{a[0]*x+a[4]*y+a[8]*z+a[12],a[1]*x+a[5]*y+a[9]*z+a[13],a[2]*x+a[6]*y+a[10]*z+a[14]}; }
        float[] point(float[] p) { return point(p[0],p[1],p[2]); }
    }
    public record Mesh(float[] xyz,int[] colours) { public int vertices() { return colours.length; } }
    public record Frame(Mesh mesh,float[] base,float[] tip) {}
    static final class Builder {
        float[] xyz=new float[4096]; int[] colours=new int[2048]; int count;
        Matrix transform=new Matrix();
        void vertex(float[] p,int colour) {
            if(count>=32768) throw new IllegalStateException("Knife geometry limit");
            if((count+1)*3>xyz.length) xyz=Arrays.copyOf(xyz,xyz.length*2);
            if(count==colours.length) colours=Arrays.copyOf(colours,colours.length*2);
            System.arraycopy(p,0,xyz,count*3,3); colours[count++]=colour;
        }
        void quad(float[] a,float[] b,float[] c,float[] d,int colour) {
            vertex(transform.point(a),colour); vertex(transform.point(b),colour); vertex(transform.point(c),colour); vertex(transform.point(d),colour);
        }
        void polygon(float[][] xy,float depth,int colour) {
            int n=xy.length;
            for(int i=1;i<n-1;++i) {
                float[] a={xy[0][0],xy[0][1],depth},b={xy[i][0],xy[i][1],depth},c={xy[i+1][0],xy[i+1][1],depth};
                quad(a,b,c,c,shade(colour,1));
                a=new float[]{a[0],a[1],-depth}; b=new float[]{b[0],b[1],-depth}; c=new float[]{c[0],c[1],-depth};
                quad(c,b,a,a,shade(colour,.65f));
            }
            for(int i=0;i<n;++i) {
                float[] a=xy[i],b=xy[(i+1)%n];
                quad(new float[]{a[0],a[1],depth},new float[]{a[0],a[1],-depth},new float[]{b[0],b[1],-depth},new float[]{b[0],b[1],depth},shade(colour,.8f));
            }
        }
        void box(float x,float y,float w,float h,float depth,int colour) {
            polygon(new float[][]{{x,y},{x+w,y},{x+w,y+h},{x,y+h}},depth,colour);
        }
        Mesh finish() { return new Mesh(Arrays.copyOf(xyz,count*3),Arrays.copyOf(colours,count)); }
    }
    static int shade(int c,float f) {
        return c&0xff000000|Math.min(255,Math.round((c>>16&255)*f))<<16|Math.min(255,Math.round((c>>8&255)*f))<<8|Math.min(255,Math.round((c&255)*f));
    }
    static int metal(String name) {
        if(name.contains("diamond")) return 0xff69ddd5;
        if(name.contains("netherite")) return 0xff867583;
        if(name.contains("golden")) return 0xffe7bd5c;
        if(name.contains("copper")) return 0xffc78f67;
        if(name.contains("wooden")) return 0xffbc9163;
        if(name.contains("stone")) return 0xffa7a8ad;
        return 0xffd9e2ea;
    }
    public static Frame build(int skin,String item,float phase,int motion,float size) {
        if(skin<0||skin>2||!Float.isFinite(size)||size<.5f||size>2) throw new IllegalArgumentException("Knife settings");
        phase=Math.max(0,Math.min(1,phase)); float t=smooth(phase),lift=(float)Math.pow(Math.sin(Math.PI*phase),2);
        Builder b=new Builder(); Matrix root=new Matrix().turn(0,-.35f,-.25f).scale(size*.75f,size*.75f,size*.75f);
        if(motion==1) {
            root.move(-.16f*lift,.13f*lift,.06f*lift);
            if(skin==0) root.turn(.3f*lift,.55f*lift,.3f*lift);
            else if(skin==1) root.turn(.4f*lift,0,(float)Math.PI*4*t);
            else root.turn(.2f*lift,(float)Math.PI*2*t,-.6f*lift);
        } else if(motion==2) root.move(0,-.22f*(1-t),0).turn(.2f*(1-t),0,(float)-Math.PI*(1-t));
        b.transform=root; int metal=metal(item),edge=shade(metal,1.15f),grip=0xff272b35,accent=0xffbda477;
        float[] base,tip;
        if(skin==0) {
            Matrix blade=new Matrix(root.a);
            if(motion!=0) blade.turn(0,0,(float)Math.PI*4*t);
            b.transform=blade;
            b.polygon(new float[][]{{-.065f,.06f},{.065f,.06f},{.075f,.58f},{0,.9f},{-.075f,.58f}},.025f,metal);
            b.polygon(new float[][]{{.045f,.1f},{.065f,.1f},{.075f,.58f},{0,.9f},{.04f,.56f}},.026f,edge);
            base=blade.point(0,.13f,0); tip=blade.point(0,.89f,0);
            for(int side:new int[]{-1,1}) {
                b.transform=new Matrix(root.a).move(side*.067f,.055f,0);
                if(motion!=0) b.transform.turn(0,0,side*(float)Math.PI*6*t);
                b.box(-.045f,-.53f,.09f,.55f,.047f,grip);
                b.box(-.026f,-.49f,.052f,.38f,.049f,shade(metal,.65f));
                for(int i=0;i<4;++i) b.box(-.018f,-.43f+i*.085f,.036f,.025f,.05f,grip);
                b.box(-.025f,-.02f,.05f,.05f,.052f,accent);
            }
        } else if(skin==1) {
            b.box(-.07f,-.4f,.14f,.39f,.06f,grip);
            for(int i=0;i<5;++i) b.box(-.075f,-.34f+i*.056f,.15f,.018f,.063f,0xff454b58);
            for(int i=0;i<24;++i) {
                double a=i*Math.PI*2/24,z=(i+1)*Math.PI*2/24;
                b.polygon(new float[][]{{.13f*(float)Math.cos(a),.05f+.13f*(float)Math.sin(a)},
                    {.13f*(float)Math.cos(z),.05f+.13f*(float)Math.sin(z)},
                    {.085f*(float)Math.cos(z),.05f+.085f*(float)Math.sin(z)},
                    {.085f*(float)Math.cos(a),.05f+.085f*(float)Math.sin(a)}},.04f,metal);
            }
            for(int i=0;i<16;++i) {
                float a=i/16f,z=(i+1)/16f;
                float x=.42f*a*a,y=-.38f-.5f*a,u=.42f*z*z,v=-.38f-.5f*z;
                float w=.14f*(1-a),w2=.14f*(1-z);
                b.polygon(new float[][]{{x-w,y},{u-w2,v},{u+.025f,v+.015f},{x+.025f,y+.015f}},.022f,metal);
            }
            base=root.point(-.04f,-.4f,0); tip=root.point(.42f,-.88f,0);
        } else {
            b.box(-.055f,-.43f,.11f,.43f,.045f,grip);
            for(int i=0;i<8;++i) b.polygon(new float[][]{{-.055f,-.4f+i*.05f},{0,-.38f+i*.05f},{.055f,-.4f+i*.05f},{0,-.42f+i*.05f}},.048f,accent);
            b.box(-.16f,-.02f,.32f,.055f,.07f,accent);
            for(int i=0;i<20;++i) {
                float y=i*.065f,v=(i+1)*.065f,x=.055f*y*y,u=.055f*v*v,w=i==19?.005f:.052f;
                b.polygon(new float[][]{{x-.052f,y},{u-w,v},{u+w,v},{x+.052f,y}},.018f,metal);
                b.polygon(new float[][]{{x+.032f,y},{u+w*.6f,v},{u+w,v},{x+.052f,y}},.019f,edge);
            }
            base=root.point(0,.08f,0); tip=root.point(.093f,1.3f,0);
        }
        return new Frame(b.finish(),base,tip);
    }
}
