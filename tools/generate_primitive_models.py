import math
import os
import sys

def write_obj(filename, vertices, normals, texcoords, faces):
    os.makedirs(os.path.dirname(os.path.abspath(filename)), exist_ok=True)
    with open(filename, 'w', encoding='utf-8') as f:
        f.write(f"# Chained Engine Primitive: {os.path.basename(filename)}\n")
        f.write("o " + os.path.splitext(os.path.basename(filename))[0] + "\n")
        for v in vertices:
            f.write(f"v {v[0]:.6f} {v[1]:.6f} {v[2]:.6f}\n")
        for vt in texcoords:
            f.write(f"vt {vt[0]:.6f} {vt[1]:.6f}\n")
        for vn in normals:
            f.write(f"vn {vn[0]:.6f} {vn[1]:.6f} {vn[2]:.6f}\n")
        for face in faces:
            # face is list of (v_idx, vt_idx, vn_idx) 1-based
            f_str = " ".join(f"{v}/{vt}/{vn}" for v, vt, vn in face)
            f.write(f"f {f_str}\n")

def generate_cube(size=1.0):
    s = size * 0.5
    # 24 vertices for 6 distinct face normals and UVs
    vertices = [
        # Front (+Z)
        (-s, -s,  s), ( s, -s,  s), ( s,  s,  s), (-s,  s,  s),
        # Back (-Z)
        ( s, -s, -s), (-s, -s, -s), (-s,  s, -s), ( s,  s, -s),
        # Top (+Y)
        (-s,  s,  s), ( s,  s,  s), ( s,  s, -s), (-s,  s, -s),
        # Bottom (-Y)
        (-s, -s, -s), ( s, -s, -s), ( s, -s,  s), (-s, -s,  s),
        # Right (+X)
        ( s, -s,  s), ( s, -s, -s), ( s,  s, -s), ( s,  s,  s),
        # Left (-X)
        (-s, -s, -s), (-s, -s,  s), (-s,  s,  s), (-s,  s, -s),
    ]
    normals = [
        ( 0,  0,  1), # Front
        ( 0,  0, -1), # Back
        ( 0,  1,  0), # Top
        ( 0, -1,  0), # Bottom
        ( 1,  0,  0), # Right
        (-1,  0,  0), # Left
    ]
    texcoords = [
        (0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)
    ]
    faces = []
    for face_idx in range(6):
        v_base = face_idx * 4 + 1
        n_idx = face_idx + 1
        faces.append([(v_base + 0, 1, n_idx), (v_base + 1, 2, n_idx), (v_base + 2, 3, n_idx)])
        faces.append([(v_base + 0, 1, n_idx), (v_base + 2, 3, n_idx), (v_base + 3, 4, n_idx)])
    return vertices, normals, texcoords, faces

def generate_plane(size=1.0):
    s = size * 0.5
    vertices = [
        (-s, 0.0, -s), ( s, 0.0, -s), ( s, 0.0,  s), (-s, 0.0,  s)
    ]
    normals = [(0.0, 1.0, 0.0)]
    texcoords = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
    faces = [
        [(1, 1, 1), (2, 2, 1), (3, 3, 1)],
        [(1, 1, 1), (3, 3, 1), (4, 4, 1)]
    ]
    return vertices, normals, texcoords, faces

def generate_sphere(radius=0.5, slices=32, stacks=16):
    vertices = []
    normals = []
    texcoords = []
    faces = []

    for i in range(stacks + 1):
        phi = math.pi * i / stacks
        v = 1.0 - (i / stacks)
        for j in range(slices + 1):
            theta = 2.0 * math.pi * j / slices
            u = j / slices
            
            x = math.sin(phi) * math.sin(theta)
            y = math.cos(phi)
            z = math.sin(phi) * math.cos(theta)
            
            vertices.append((x * radius, y * radius, z * radius))
            normals.append((x, y, z))
            texcoords.append((u, v))

    for i in range(stacks):
        for j in range(slices):
            p1 = i * (slices + 1) + j + 1
            p2 = p1 + 1
            p3 = (i + 1) * (slices + 1) + j + 1
            p4 = p3 + 1
            if i != 0:
                faces.append([(p1, p1, p1), (p3, p3, p3), (p2, p2, p2)])
            if i != (stacks - 1):
                faces.append([(p2, p2, p2), (p3, p3, p3), (p4, p4, p4)])

    return vertices, normals, texcoords, faces

def generate_hemisphere(radius=0.5, slices=32, stacks=16):
    vertices = []
    normals = []
    texcoords = []
    faces = []

    # Dome
    for i in range(stacks + 1):
        phi = 0.5 * math.pi * i / stacks
        v = 1.0 - (i / stacks)
        for j in range(slices + 1):
            theta = 2.0 * math.pi * j / slices
            u = j / slices
            x = math.cos(phi) * math.sin(theta)
            y = math.sin(phi)
            z = math.cos(phi) * math.cos(theta)
            vertices.append((x * radius, y * radius, z * radius))
            normals.append((x, y, z))
            texcoords.append((u, v))

    for i in range(stacks):
        for j in range(slices):
            p1 = i * (slices + 1) + j + 1
            p2 = p1 + 1
            p3 = (i + 1) * (slices + 1) + j + 1
            p4 = p3 + 1
            faces.append([(p1, p1, p1), (p2, p2, p2), (p3, p3, p3)])
            faces.append([(p2, p2, p2), (p4, p4, p4), (p3, p3, p3)])

    # Bottom flat disc
    center_idx = len(vertices) + 1
    vertices.append((0.0, 0.0, 0.0))
    normals.append((0.0, -1.0, 0.0))
    texcoords.append((0.5, 0.5))

    base_idx = len(vertices) + 1
    for j in range(slices + 1):
        theta = 2.0 * math.pi * j / slices
        x = math.sin(theta)
        z = math.cos(theta)
        vertices.append((x * radius, 0.0, z * radius))
        normals.append((0.0, -1.0, 0.0))
        texcoords.append((x * 0.5 + 0.5, z * 0.5 + 0.5))

    for j in range(slices):
        p1 = base_idx + j
        p2 = base_idx + j + 1
        faces.append([(center_idx, center_idx, center_idx), (p2, p2, p2), (p1, p1, p1)])

    return vertices, normals, texcoords, faces

def generate_cylinder(radius=0.5, height=1.0, slices=32):
    half_h = height * 0.5
    vertices = []
    normals = []
    texcoords = []
    faces = []

    # Side vertices
    for j in range(slices + 1):
        theta = 2.0 * math.pi * j / slices
        u = j / slices
        nx = math.sin(theta)
        nz = math.cos(theta)
        
        # Top vertex
        vertices.append((nx * radius, half_h, nz * radius))
        normals.append((nx, 0.0, nz))
        texcoords.append((u, 1.0))
        
        # Bottom vertex
        vertices.append((nx * radius, -half_h, nz * radius))
        normals.append((nx, 0.0, nz))
        texcoords.append((u, 0.0))

    for j in range(slices):
        p1 = j * 2 + 1
        p2 = p1 + 1
        p3 = (j + 1) * 2 + 1
        p4 = p3 + 1
        faces.append([(p1, p1, p1), (p2, p2, p2), (p3, p3, p3)])
        faces.append([(p2, p2, p2), (p4, p4, p4), (p3, p3, p3)])

    # Top Cap
    top_center = len(vertices) + 1
    vertices.append((0.0, half_h, 0.0))
    normals.append((0.0, 1.0, 0.0))
    texcoords.append((0.5, 0.5))

    top_ring = len(vertices) + 1
    for j in range(slices + 1):
        theta = 2.0 * math.pi * j / slices
        x = math.sin(theta)
        z = math.cos(theta)
        vertices.append((x * radius, half_h, z * radius))
        normals.append((0.0, 1.0, 0.0))
        texcoords.append((x * 0.5 + 0.5, z * 0.5 + 0.5))

    for j in range(slices):
        p1 = top_ring + j
        p2 = top_ring + j + 1
        faces.append([(top_center, top_center, top_center), (p1, p1, p1), (p2, p2, p2)])

    # Bottom Cap
    bot_center = len(vertices) + 1
    vertices.append((0.0, -half_h, 0.0))
    normals.append((0.0, -1.0, 0.0))
    texcoords.append((0.5, 0.5))

    bot_ring = len(vertices) + 1
    for j in range(slices + 1):
        theta = 2.0 * math.pi * j / slices
        x = math.sin(theta)
        z = math.cos(theta)
        vertices.append((x * radius, -half_h, z * radius))
        normals.append((0.0, -1.0, 0.0))
        texcoords.append((x * 0.5 + 0.5, z * 0.5 + 0.5))

    for j in range(slices):
        p1 = bot_ring + j
        p2 = bot_ring + j + 1
        faces.append([(bot_center, bot_center, bot_center), (p2, p2, p2), (p1, p1, p1)])

    return vertices, normals, texcoords, faces

def generate_capsule(radius=0.5, height=2.0, slices=32, stacks=8):
    cylinder_h = max(0.0, height - 2.0 * radius)
    half_cyl_h = cylinder_h * 0.5
    vertices = []
    normals = []
    texcoords = []
    faces = []

    # Top hemisphere
    for i in range(stacks + 1):
        phi = 0.5 * math.pi * (stacks - i) / stacks
        v = 1.0 - (0.5 * (stacks - i) / (stacks * 2))
        for j in range(slices + 1):
            theta = 2.0 * math.pi * j / slices
            u = j / slices
            x = math.cos(phi) * math.sin(theta)
            y = math.sin(phi)
            z = math.cos(phi) * math.cos(theta)
            vertices.append((x * radius, half_cyl_h + y * radius, z * radius))
            normals.append((x, y, z))
            texcoords.append((u, v))

    top_count = (stacks + 1) * (slices + 1)

    # Bottom hemisphere
    for i in range(stacks + 1):
        phi = 0.5 * math.pi * i / stacks
        v = 0.5 * (stacks - i) / (stacks * 2)
        for j in range(slices + 1):
            theta = 2.0 * math.pi * j / slices
            u = j / slices
            x = math.cos(phi) * math.sin(theta)
            y = -math.sin(phi)
            z = math.cos(phi) * math.cos(theta)
            vertices.append((x * radius, -half_cyl_h + y * radius, z * radius))
            normals.append((x, y, z))
            texcoords.append((u, v))

    # Faces for top hemisphere
    for i in range(stacks):
        for j in range(slices):
            p1 = i * (slices + 1) + j + 1
            p2 = p1 + 1
            p3 = (i + 1) * (slices + 1) + j + 1
            p4 = p3 + 1
            faces.append([(p1, p1, p1), (p3, p3, p3), (p2, p2, p2)])
            faces.append([(p2, p2, p2), (p3, p3, p3), (p4, p4, p4)])

    # Faces for cylinder side connecting the two
    top_base = stacks * (slices + 1) + 1
    bot_top = top_count + 1
    for j in range(slices):
        p1 = top_base + j
        p2 = p1 + 1
        p3 = bot_top + j
        p4 = p3 + 1
        faces.append([(p1, p1, p1), (p3, p3, p3), (p2, p2, p2)])
        faces.append([(p2, p2, p2), (p3, p3, p3), (p4, p4, p4)])

    # Faces for bottom hemisphere
    for i in range(stacks):
        for j in range(slices):
            p1 = top_count + i * (slices + 1) + j + 1
            p2 = p1 + 1
            p3 = top_count + (i + 1) * (slices + 1) + j + 1
            p4 = p3 + 1
            faces.append([(p1, p1, p1), (p3, p3, p3), (p2, p2, p2)])
            faces.append([(p2, p2, p2), (p3, p3, p3), (p4, p4, p4)])

    return vertices, normals, texcoords, faces

def generate_cone(radius=0.5, height=1.0, slices=32):
    half_h = height * 0.5
    vertices = []
    normals = []
    texcoords = []
    faces = []

    # Apex
    apex_idx = 1
    vertices.append((0.0, half_h, 0.0))
    normals.append((0.0, 1.0, 0.0))
    texcoords.append((0.5, 1.0))

    # Base ring for sides
    side_ring = 2
    slope = radius / height
    normal_len = math.sqrt(1.0 + slope * slope)
    for j in range(slices + 1):
        theta = 2.0 * math.pi * j / slices
        u = j / slices
        nx = math.sin(theta) / normal_len
        ny = slope / normal_len
        nz = math.cos(theta) / normal_len
        x = math.sin(theta) * radius
        z = math.cos(theta) * radius
        vertices.append((x, -half_h, z))
        normals.append((nx, ny, nz))
        texcoords.append((u, 0.0))

    for j in range(slices):
        p1 = side_ring + j
        p2 = side_ring + j + 1
        faces.append([(apex_idx, apex_idx, apex_idx), (p1, p1, p1), (p2, p2, p2)])

    # Bottom Cap
    bot_center = len(vertices) + 1
    vertices.append((0.0, -half_h, 0.0))
    normals.append((0.0, -1.0, 0.0))
    texcoords.append((0.5, 0.5))

    bot_ring = len(vertices) + 1
    for j in range(slices + 1):
        theta = 2.0 * math.pi * j / slices
        x = math.sin(theta)
        z = math.cos(theta)
        vertices.append((x * radius, -half_h, z * radius))
        normals.append((0.0, -1.0, 0.0))
        texcoords.append((x * 0.5 + 0.5, z * 0.5 + 0.5))

    for j in range(slices):
        p1 = bot_ring + j
        p2 = bot_ring + j + 1
        faces.append([(bot_center, bot_center, bot_center), (p2, p2, p2), (p1, p1, p1)])

    return vertices, normals, texcoords, faces

def generate_torus(r_major=0.75, r_minor=0.25, segments=32, sides=16):
    vertices = []
    normals = []
    texcoords = []
    faces = []

    for i in range(segments + 1):
        u = i / segments
        theta = 2.0 * math.pi * u
        cos_t = math.cos(theta)
        sin_t = math.sin(theta)

        for j in range(sides + 1):
            v = j / sides
            phi = 2.0 * math.pi * v
            cos_p = math.cos(phi)
            sin_p = math.sin(phi)

            x = (r_major + r_minor * cos_p) * sin_t
            y = r_minor * sin_p
            z = (r_major + r_minor * cos_p) * cos_t

            nx = cos_p * sin_t
            ny = sin_p
            nz = cos_p * cos_t

            vertices.append((x, y, z))
            normals.append((nx, ny, nz))
            texcoords.append((u, v))

    for i in range(segments):
        for j in range(sides):
            p1 = i * (sides + 1) + j + 1
            p2 = p1 + 1
            p3 = (i + 1) * (sides + 1) + j + 1
            p4 = p3 + 1
            faces.append([(p1, p1, p1), (p2, p2, p2), (p3, p3, p3)])
            faces.append([(p2, p2, p2), (p4, p4, p4), (p3, p3, p3)])

    return vertices, normals, texcoords, faces

def main():
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    target_dirs = [
        os.path.join(root_dir, "resources", "primitives"),
        os.path.join(root_dir, "game", "chaineddecos", "assets", "models", "primitives")
    ]
    
    generators = {
        "Cube.obj": generate_cube,
        "Plane.obj": generate_plane,
        "Sphere.obj": generate_sphere,
        "Hemisphere.obj": generate_hemisphere,
        "Cylinder.obj": generate_cylinder,
        "Capsule.obj": generate_capsule,
        "Cone.obj": generate_cone,
        "Torus.obj": generate_torus
    }

    for d in target_dirs:
        for name, gen in generators.items():
            path = os.path.join(d, name)
            v, n, vt, f = gen()
            write_obj(path, v, n, vt, f)
            print(f"Generated: {path} ({len(v)} vertices, {len(f)} triangles)")

if __name__ == "__main__":
    main()
