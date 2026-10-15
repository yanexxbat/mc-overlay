"""Software QA preview of production OBJ exports, including vertex-alpha glow.
Run knife_models first. No blur, fabricated detail or external model assets.
"""
from pathlib import Path
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

def mesh(path):
    vertices=[]; faces=[]
    for line in path.read_text().splitlines():
        if line.startswith('v '): vertices.append(list(map(float,line.split()[1:])))
        elif line.startswith('f '): faces.append([int(v)-1 for v in line.split()[1:]])
    return np.asarray(vertices),faces

def draw_mesh(canvas,depth,vertices,faces,scale,offset,additive=False):
    xy=vertices[:,:2]*[scale,-scale]+offset
    for face in faces:
        for ids in ([face[0],face[1],face[2]],[face[0],face[2],face[3]]):
            points=xy[ids]; colours=vertices[ids,3:7]; z=vertices[ids,2]
            lo=np.maximum(np.floor(points.min(0)).astype(int),0)
            hi=np.minimum(np.ceil(points.max(0)).astype(int),[canvas.shape[1]-1,canvas.shape[0]-1])
            if np.any(lo>hi):continue
            a,b,c=points;den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
            if abs(den)<1e-8:continue
            yy,xx=np.mgrid[lo[1]:hi[1]+1,lo[0]:hi[0]+1];xx=xx+.5;yy=yy+.5
            w0=((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/den
            w1=((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/den;w2=1-w0-w1
            zz=w0*z[0]+w1*z[1]+w2*z[2]
            d=depth[lo[1]:hi[1]+1,lo[0]:hi[0]+1];part=canvas[lo[1]:hi[1]+1,lo[0]:hi[0]+1]
            # Half-open edge inclusion avoids additive double hits on a shared diagonal.
            def edge(weight,start,end):
                direction=(end-start)*(1 if den>0 else -1)
                inclusive=direction[1]<0 or (abs(direction[1])<1e-10 and direction[0]>0)
                return (weight>1e-9)|((abs(weight)<=1e-9)&inclusive)
            inside=edge(w0,b,c)&edge(w1,c,a)&edge(w2,a,b)&(zz>=d-1e-6)
            rgba=w0[...,None]*colours[0]+w1[...,None]*colours[1]+w2[...,None]*colours[2]
            if additive:part[inside]+=rgba[inside,:3]*rgba[inside,3,None]*255
            else:part[inside]=rgba[inside,:3]*255;d[inside]=zz[inside]

if __name__=='__main__':
    source=Path(sys.argv[1] if len(sys.argv)>1 else 'build/knife-preview')
    out=source/'models-glow-preview.png';w,h=600,620
    board=Image.new('RGB',(w*3,h*2+110),(14,18,25));ink=ImageDraw.Draw(board)
    font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',23)
    small=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',18)
    ink.text((30,14),'Production mesh preview · Butterfly / Karambit / Katana',font=font,fill='#e4e8ee')
    ink.text((30,49),'Synthetic orthographic render, not an in-game capture. Top: solid geometry. Bottom: actual additive glow vertices.',font=small,fill='#98a3b4')
    for col,name in enumerate(('butterfly','karambit','katana')):
        v,f=mesh(source/(name+'.obj'));g,gf=mesh(source/(name+'-glow.obj'))
        bounds=v[:,:2].min(0),v[:,:2].max(0)
        scale=min((w-150)/(bounds[1][0]-bounds[0][0]),(h-100)/(bounds[1][1]-bounds[0][1]))
        middle=(bounds[0]+bounds[1])/2;offset=np.array([w/2,h/2])+[-middle[0]*scale,middle[1]*scale]
        for row in range(2):
            canvas=np.zeros((h,w,3),dtype=float)+[14,18,25];depth=np.full((h,w),-np.inf)
            draw_mesh(canvas,depth,v,f,scale,offset)
            if row:draw_mesh(canvas,depth,g,gf,scale,offset,True)
            board.paste(Image.fromarray(np.clip(canvas,0,255).astype('uint8')),(col*w,85+row*h))
            ink.text((col*w+24,90+row*h),name.title()+(' · Glow' if row else ' · Solid'),font=font,fill='#e4e8ee')
    board.save(out);print(out)
