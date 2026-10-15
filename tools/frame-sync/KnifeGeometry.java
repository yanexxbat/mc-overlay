package mcov;

import java.util.*;

/** Original, cached solid models. No third-party assets or game resources. */
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
        Matrix move(float x,float y,float z) { Matrix b=new Matrix(); b.a[12]=x;b.a[13]=y;b.a[14]=z;return mul(b); }
        Matrix scale(float x,float y,float z) { Matrix b=new Matrix();b.a[0]=x;b.a[5]=y;b.a[10]=z;return mul(b); }
        Matrix turn(float x,float y,float z) {
            for(int axis=0;axis<3;++axis) {
                float v=axis==0?x:axis==1?y:z,c=(float)Math.cos(v),s=(float)Math.sin(v);Matrix b=new Matrix();
                int i=(axis+1)%3,j=(axis+2)%3;b.a[i*4+i]=b.a[j*4+j]=c;b.a[i*4+j]=s;b.a[j*4+i]=-s;mul(b);
            }
            return this;
        }
        public float[] point(float x,float y,float z) { return new float[]{a[0]*x+a[4]*y+a[8]*z+a[12],a[1]*x+a[5]*y+a[9]*z+a[13],a[2]*x+a[6]*y+a[10]*z+a[14]}; }
        float[] point(float[] p) { return point(p[0],p[1],p[2]); }
    }
    public record Mesh(float[] xyz,int[] colours) { public int vertices() { return colours.length; } }
    public record Frame(Mesh mesh,float[] base,float[] tip,float[] glowSegments) {}
    static final class Builder {
        float[] xyz=new float[4096];int[] colours=new int[2048];int count;
        Matrix transform=new Matrix();
        void vertex(float[] p,int colour) {
            if(count>=32768) throw new IllegalStateException("Knife geometry limit");
            if((count+1)*3>xyz.length) xyz=Arrays.copyOf(xyz,xyz.length*2);
            if(count==colours.length) colours=Arrays.copyOf(colours,colours.length*2);
            System.arraycopy(p,0,xyz,count*3,3);colours[count++]=colour;
        }
        void quad(float[] a,float[] b,float[] c,float[] d,int colour) { quad(a,b,c,d,colour,colour,colour,colour); }
        void quad(float[] a,float[] b,float[] c,float[] d,int ca,int cb,int cc,int cd) {
            vertex(transform.point(a),ca);vertex(transform.point(b),cb);vertex(transform.point(c),cc);vertex(transform.point(d),cd);
        }
        void append(Mesh mesh,Matrix matrix) {
            for(int i=0;i<mesh.vertices();++i) vertex(matrix.point(mesh.xyz[i*3],mesh.xyz[i*3+1],mesh.xyz[i*3+2]),mesh.colours[i]);
        }
        Mesh finish() { return new Mesh(Arrays.copyOf(xyz,count*3),Arrays.copyOf(colours,count)); }
    }
    static int shade(int c,float f) {
        return c&0xff000000|Math.min(255,Math.round((c>>16&255)*f))<<16|Math.min(255,Math.round((c>>8&255)*f))<<8|Math.min(255,Math.round((c&255)*f));
    }
    static int metal(String name) {
        if(name.contains("diamond"))return 0xff69ddd5;if(name.contains("netherite"))return 0xff867583;
        if(name.contains("golden"))return 0xffe7bd5c;if(name.contains("copper"))return 0xffc78f67;
        if(name.contains("wooden"))return 0xffbc9163;if(name.contains("stone"))return 0xffa7a8ad;return 0xffd9e2ea;
    }
    private record Model(Mesh blade,Mesh[] handles,float[] edges,float[] base,float[] tip) {}
    // Seven canonical materials x three knives. Shape construction never runs per frame.
    private static final Map<Long,Model> CACHE=new HashMap<>();
    private static synchronized Model model(int skin,int colour) {
        long key=((long)skin<<32)|(colour&0xffffffffL);
        return CACHE.computeIfAbsent(key,k->create(skin,colour));
    }
    private static float[] p(float x,float y,float z) { return new float[]{x,y,z}; }
    /** Rounded elliptical grip, end caps included, with machined circumferential grooves. */
    private static void grip(Builder b,float x,float low,float high,float width,float depth,int colour,int grooves) {
        int rings=32,sides=24;
        for(int i=0;i<rings;++i) for(int j=0;j<sides;++j) {
            float[][] q=new float[4][];int[] ink=new int[4];
            for(int v=0;v<4;++v) {
                int r=i+(v==1||v==2?1:0),s=j+(v>=2?1:0);float t=r/(float)rings;
                double angle=s*Math.PI*2/sides;float cap=(float)Math.sqrt(Math.max(0,1-Math.pow(Math.max(0,Math.abs(2*t-1)-.8)/.2,2)));
                float groove=grooves==0?1:1-.06f*(float)Math.pow(.5+.5*Math.cos(t*grooves*Math.PI*2),12);
                q[v]=p(x+width*cap*groove*(float)Math.cos(angle),low+(high-low)*t,depth*cap*groove*(float)Math.sin(angle));
                ink[v]=shade(colour,(float)(.62+.34*Math.max(0,Math.sin(angle))+.16*Math.max(0,Math.cos(angle))));
            }
            b.quad(q[0],q[1],q[2],q[3],ink[0],ink[1],ink[2],ink[3]);
        }
    }
    /** Smooth torus in the XY plane; also used for screw heads and collars. */
    private static void ring(Builder b,float x,float y,float z,float radius,float tube,int colour,int segments) {
        for(int i=0;i<segments;++i)for(int j=0;j<8;++j) {
            float[][] q=new float[4][];int[] ink=new int[4];
            for(int v=0;v<4;++v) {
                double a=(i+(v>=2?1:0))*Math.PI*2/segments,t=(j+(v==1||v==2?1:0))*Math.PI*2/8;
                float r=radius+tube*(float)Math.cos(t);
                q[v]=p(x+r*(float)Math.cos(a),y+r*(float)Math.sin(a),z+tube*(float)Math.sin(t));
                ink[v]=shade(colour,(float)(.65+.4*Math.max(0,Math.sin(t))+.12*Math.cos(a)));
            }
            b.quad(q[0],q[3],q[2],q[1],ink[0],ink[3],ink[2],ink[1]);
        }
    }
    private static void screw(Builder b,float x,float y,int colour) {
        for(int side:new int[]{-1,1}) {
            ring(b,x,y,side*.046f,.012f,.0045f,colour,16);
            // Recessed, cross-shaped driver slot, visible from either side.
            float z=side*.047f;
            b.quad(p(x-.008f,y-.002f,z),p(x+.008f,y-.002f,z),p(x+.008f,y+.002f,z),p(x-.008f,y+.002f,z),0xff121820);
        }
    }
    private static float[] centre(int skin,float t) {
        if(skin==1)return p(.42f*t*t,-.37f-.52f*t,0);
        if(skin==2)return p(.055f*1.3f*t*1.3f*t,.025f+1.275f*t,0);
        return p(.012f*(float)Math.sin(t*Math.PI),.065f+.84f*t,0);
    }
    private static float width(int skin,float t) {
        if(skin==1)return .072f*(float)Math.pow(1-t,.65);
        if(skin==2)return .053f*(t<.87f?1:Math.max(0,(1-t)/.13f));
        return .068f*(t<.67f?1+.12f*(float)Math.sin(t*4):Math.max(0,(1-t)/.33f));
    }
    private static Model create(int skin,int metal) {
        Builder blade=new Builder();ArrayList<Float> edges=new ArrayList<>();int steps=64;
        // Eight-sided lenticular section: spine, broad satin flats, polished bevels, cutting edges.
        float[] crossX={1,.72f,0,-.72f,-1,-.72f,0,.72f},crossZ={0,.72f,1,.72f,0,-.72f,-1,-.72f};
        for(int i=0;i<steps;++i) {
            float t=i/(float)steps,u=(i+1)/(float)steps;
            for(int side:new int[]{-1,1}) for(float v:new float[]{t,u}) {
                float[] c=centre(skin,v);edges.add(c[0]+side*width(skin,v));edges.add(c[1]);edges.add(0.f);
            }
            for(int j=0;j<8;++j) {
                float[][] q=new float[4][];int[] ink=new int[4];
                for(int v=0;v<4;++v) {
                    float f=(v==1||v==2)?u:t;int k=(j+(v>=2?1:0))%8;float[] c=centre(skin,f);
                    float taper=f>.9f?(1-f)*10:1;
                    q[v]=p(c[0]+crossX[k]*width(skin,f),c[1],crossZ[k]*.023f*taper);
                    float polish=k==0||k==4?1.22f:k==2?1.02f:k==6?.63f:k<4?.82f:.52f;
                    // Subtle longitudinal satin variation, baked into the cached mesh.
                    ink[v]=shade(metal,polish*(.985f+.015f*(float)Math.sin(f*390)));
                }
                blade.quad(q[0],q[1],q[2],q[3],ink[0],ink[1],ink[2],ink[3]);
            }
        }
        ArrayList<Mesh> handles=new ArrayList<>();int grip=0xff252e3b,accent=0xffbda477;
        if(skin==0) {
            // Separate, rounded handle assemblies retain independent butterfly pivots.
            for(int side:new int[]{-1,1}) {
                Builder h=new Builder();grip(h,0,-.54f,.035f,.043f,.039f,grip,12);
                for(int i=0;i<6;++i) {
                    float y=-.445f+i*.065f;
                    // Dark pocket inset with a polished machined rim.
                    ring(h,0,y,.034f,.014f,.003f,shade(metal,.75f),16);
                    h.quad(p(-.012f,y-.01f,.035f),p(.012f,y-.01f,.035f),p(.012f,y+.01f,.035f),p(-.012f,y+.01f,.035f),0xff101722);
                }
                screw(h,0,.006f,metal);screw(h,0,-.505f,metal);handles.add(h.finish());
            }
        } else {
            Builder h=new Builder();
            if(skin==1) {
                grip(h,-.005f,-.425f,-.045f,.069f,.049f,grip,9);
                ring(h,0,.054f,0,.105f,.022f,metal,96);screw(h,0,-.1f,metal);screw(h,0,-.34f,metal);
            } else {
                grip(h,0,-.44f,-.015f,.055f,.042f,grip,0);
                // Raised braid wraps cross both faces, ending in compact metal collars.
                for(int i=0;i<10;++i)for(int side:new int[]{-1,1}) {
                    float y=-.405f+i*.037f,z=side*.0425f;
                    h.quad(p(-.052f,y,z),p(-.052f,y+.013f,z),p(.052f,y+.043f,z),p(.052f,y+.030f,z),shade(accent,side>0?.91f:.65f));
                    h.quad(p(.052f,y,z),p(.052f,y+.013f,z),p(-.052f,y+.043f,z),p(-.052f,y+.030f,z),shade(accent,side>0?1.05f:.72f));
                }
                // Elliptical, rounded tsuba, oriented across the grip.
                Matrix old=h.transform;h.transform=new Matrix().turn((float)Math.PI/2,0,0).scale(1.5f,1,1);
                ring(h,0,0,0,.082f,.018f,accent,64);h.transform=old;
                grip(h,0,-.445f,-.413f,.057f,.045f,accent,0);grip(h,0,-.035f,.025f,.06f,.046f,accent,0);
            }
            handles.add(h.finish());
        }
        float[] lines=new float[edges.size()];for(int i=0;i<lines.length;++i)lines[i]=edges.get(i);
        return new Model(blade.finish(),handles.toArray(Mesh[]::new),lines,centre(skin,.06f),centre(skin,1));
    }
    public static Frame build(int skin,String item,float phase,int motion,float size) {
        if(skin<0||skin>2||!Float.isFinite(size)||size<.5f||size>2||!Float.isFinite(phase))throw new IllegalArgumentException("Knife settings");
        phase=Math.max(0,Math.min(1,phase));float t=smooth(phase),lift=(float)Math.pow(Math.sin(Math.PI*phase),2);
        Model cached=model(skin,metal(item));Matrix root=new Matrix().turn(0,-.35f,-.25f).scale(size*.75f,size*.75f,size*.75f);
        if(motion==1) {
            root.move(-.16f*lift,.13f*lift,.06f*lift);
            if(skin==0)root.turn(.3f*lift,.55f*lift,.3f*lift);else if(skin==1)root.turn(.4f*lift,0,(float)Math.PI*4*t);else root.turn(.2f*lift,(float)Math.PI*2*t,-.6f*lift);
        }else if(motion==2)root.move(0,-.22f*(1-t),0).turn(.2f*(1-t),0,(float)-Math.PI*(1-t));
        Matrix blade=new Matrix(root.a);if(skin==0&&motion!=0)blade.turn(0,0,(float)Math.PI*4*t);
        Builder b=new Builder();b.append(cached.blade,blade);
        for(int i=0;i<cached.handles.length;++i) {
            Matrix handle=new Matrix(root.a);
            if(skin==0) {int side=i==0?-1:1;handle.move(side*.062f,.055f,0);if(motion!=0)handle.turn(0,0,side*(float)Math.PI*6*t);}
            b.append(cached.handles[i],handle);
        }
        float[] lines=new float[cached.edges.length];for(int i=0;i<lines.length;i+=3)System.arraycopy(blade.point(cached.edges[i],cached.edges[i+1],cached.edges[i+2]),0,lines,i,3);
        return new Frame(b.finish(),blade.point(cached.base),blade.point(cached.tip),lines);
    }
    private record Position(float x,float y,float z) {}
    private static final class Joint {
        final float[] p;float nx,ny,dx,dy;int degree;
        Joint(float[] point) { p=point; }
        void connect(float x,float y,float direction) { nx+=-y;ny+=x;dx+=x*direction;dy+=y*direction;++degree; }
        float[] offset(float width,float amount) { float length=(float)Math.hypot(nx,ny);float x=length<1e-7f?1:nx/length,y=length<1e-7f?0:ny/length;return new float[]{p[0]+x*width*amount,p[1]+y*width*amount,p[2]+.025f}; }
    }
    private record Segment(Joint a,Joint z) {}
    private static int glowInk(int colour,float strength,float radius) {
        float falloff=(float)((Math.exp(-radius*radius*5)-Math.exp(-5))/(1-Math.exp(-5)));
        return (colour&0xffffff)|Math.min(255,Math.round(90*strength*Math.max(0,falloff)))<<24;
    }
    /** Camera-facing additive Gaussian ribbons with shared curve joins and rounded tips. */
    static Mesh glow(float[] segments,Matrix view,float width,float strength,int colour) { return glow(segments,view,width,strength,colour,true); }
    static Mesh glow(float[] segments,Matrix view,float width,float strength,int colour,boolean caps) {
        Builder b=new Builder();if(strength<=0)return b.finish();
        Map<Position,Joint> joints=new LinkedHashMap<>();List<Segment> lines=new ArrayList<>();
        for(int i=0;i<segments.length;i+=6) {
            float[] a=view.point(segments[i],segments[i+1],segments[i+2]),z=view.point(segments[i+3],segments[i+4],segments[i+5]);
            float dx=z[0]-a[0],dy=z[1]-a[1],length=(float)Math.hypot(dx,dy);if(length<1e-7f)continue;
            Joint first=joints.computeIfAbsent(new Position(a[0],a[1],a[2]),key->new Joint(a));
            Joint last=joints.computeIfAbsent(new Position(z[0],z[1],z[2]),key->new Joint(z));
            first.connect(dx/length,dy/length,1);last.connect(dx/length,dy/length,-1);lines.add(new Segment(first,last));
        }
        for(Segment line:lines)for(int band=-10;band<10;++band) {
            float v=band/10f,w=(band+1)/10f;
            int ca=glowInk(colour,strength,v),cz=glowInk(colour,strength,w);
            b.quad(line.a.offset(width,v),line.z.offset(width,v),line.z.offset(width,w),line.a.offset(width,w),ca,ca,cz,cz);
        }
        if(caps)for(Joint joint:joints.values()) {
            // Endpoints and acute cutting tips; smooth interior joints need no overlapping disks.
            if(joint.degree>1&&Math.hypot(joint.dx,joint.dy)/joint.degree<.6)continue;
            for(int ring=0;ring<10;++ring)for(int side=0;side<32;++side) {
                float[][] q=new float[4][];int[] ink=new int[4];
                for(int v=0;v<4;++v) {
                    float radius=(ring+(v==1||v==2?1:0))/10f;double angle=(side+(v>=2?1:0))*Math.PI*2/32;
                    q[v]=p(joint.p[0]+width*radius*(float)Math.cos(angle),joint.p[1]+width*radius*(float)Math.sin(angle),joint.p[2]+.025f);
                    ink[v]=glowInk(colour,strength,radius);
                }
                b.quad(q[0],q[1],q[2],q[3],ink[0],ink[1],ink[2],ink[3]);
            }
        }
        return b.finish();
    }
}
