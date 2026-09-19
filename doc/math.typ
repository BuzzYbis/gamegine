#set page(
  paper: "a4",
  margin: (x: 2.3cm, top: 2.4cm, bottom: 2.2cm),
  numbering: "1",
  number-align: center,
)
#set text(font: "Libertinus Serif", size: 10.5pt, lang: "en")
#set par(justify: true, leading: 0.62em, spacing: 1.1em)
#show math.equation: set text(font: "New Computer Modern Math")
#set math.equation(numbering: "(1)")
#show raw: set text(font: "DejaVu Sans Mono", size: 8.8pt)
#set heading(numbering: "1.1")

#show heading.where(level: 1): it => block(sticky: true, above: 2.2em, below: 1.0em, width: 100%)[
  #set text(size: 15pt, weight: 600)
  #block(below: 0.45em)[#if it.numbering != none { counter(heading).display(); h(0.6em) } #it.body]
  #line(length: 100%, stroke: 0.7pt + black)
]
#show heading.where(level: 2): it => block(sticky: true, above: 1.5em, below: 0.75em)[
  #set text(size: 11.5pt, weight: 600)
  #counter(heading).display() #h(0.5em) #it.body
]
#show heading.where(level: 3): it => block(sticky: true, above: 1.2em, below: 0.6em)[
  #set text(size: 10.5pt, weight: 600, style: "italic")
  #it.body
]

#let sheet(cols, ..cells) = {
  set text(size: 9.6pt)
  show table.cell.where(y: 0): strong
  block(breakable: false, table(
    columns: cols,
    inset: (x: 6pt, y: 5.5pt),
    align: left + top,
    stroke: (x, y) => if y == 0 { (bottom: 0.7pt + black) } else { (bottom: 0.3pt + luma(195)) },
    ..cells,
  ))
}

#let warn(body) = block(
  breakable: false,
  width: 100%,
  inset: (left: 10pt, right: 9pt, y: 8pt),
  stroke: (left: 2pt + rgb("#8c2f14")),
  fill: rgb("#faf5f2"),
  body,
)

#let result(body) = block(
  breakable: false,
  width: 100%,
  inset: (left: 10pt, right: 9pt, y: 8pt),
  stroke: (left: 2pt + rgb("#16314d")),
  fill: rgb("#f3f6f9"),
  body,
)

#align(center)[
  #v(1.2cm)
  #text(size: 22pt, weight: 600)[Mathematics of the geometry pipeline]
  #v(0.5em)
  #text(size: 11pt, fill: luma(90))[
    Reference for a virtualized geometry engine \
    Vulkan 1.4 and Metal 4, reversed-Z, cluster DAG, out-of-core residency
  ]
  #v(1.4cm)
]

Every result here is something the engine depends on being correct. Sections run roughly in the order the material bites: conventions first, then selection, then construction, then rasterization, then the arithmetic that sizes the system and the statistics that measure it.

Vectors are columns, $bold(v)^T$ is the transpose, $norm(bold(v))$ the Euclidean norm, and a point in homogeneous form is $tilde(bold(v)) = (x, y, z, 1)^T$. Companion documents: architecture, roadmap, benchmarks, CI, references.

#v(0.8cm)
#outline(title: [Contents], depth: 1, indent: 1.2em)
#pagebreak()

= Conventions, fixed once

Get these wrong and every downstream formula silently flips sign on one backend.

#sheet(
  (auto, 1fr),
  [Convention], [Choice],
  [Handedness], [Right-handed world],
  [Depth range], [$[0, 1]$],
  [Depth direction], [*Reversed Z*: near $= 1$, far $= 0$, clear $= 0$],
  [Matrix storage], [Column-major, column-vector convention: $bold(v)' = bold(M) bold(v)$],
  [Winding], [Counter-clockwise front faces, matched explicitly between backends],
  [Texture origin], [Recorded per backend, normalized in the RHI and not in shaders],
)

A transform is written $bold(M) = [bold(A) bar bold(t)]$ with $bold(A)$ the $3 times 3$ linear part and $bold(t)$ the translation.

== Normals are covectors

A normal does not transform by $bold(A)$. Preserving orthogonality to the surface requires the inverse transpose:

$ bold(n)' = (bold(A)^(-1))^T bold(n), quad "then renormalize" $

For a rigid transform $(bold(A)^(-1))^T = bold(A)$, which is exactly why this mistake survives every test until the first nonuniform scale appears in the scene.

= The scale bound

The LOD error formula needs one number per instance: how much can this transform stretch a distance? That is the operator 2-norm of the linear part, which equals its largest singular value.

#result[
$ s := norm(bold(A))_2 = sigma_max (bold(A)) = sqrt(lambda_max (bold(A)^T bold(A))) $
]

where $lambda_max$ is the largest eigenvalue of the symmetric positive semi-definite matrix $bold(A)^T bold(A)$.

#warn[
For a declared transform $bold(A) = bold(R) "diag"(s_x,s_y,s_z)$ with orthogonal $bold(R)$, $max(|s_x|,|s_y|,|s_z|)$ is exact: rotation does not change singular values. It is not a general bound for shear or compositions of rotated nonuniform scales. Do not infer stretch from matrix diagonal entries or the largest column length.
]

== Cheap conservative substitutes

An SVD per instance per frame is not necessary. Both of these bound $sigma_max$ from above:

$ sigma_max (bold(A)) <= norm(bold(A))_F = sqrt(sum_(i,j) a_(i j)^2) $

$ sigma_max (bold(A)) <= sqrt(norm(bold(A))_1 dot norm(bold(A))_infinity) $

The Frobenius bound overestimates by at most $sqrt(3)$ for a $3 times 3$ matrix. Overestimating is safe — it refines more than necessary. Underestimating is a correctness bug that shows up as popping and as silhouette error beyond the declared bound.

== Transformed bounding spheres

A sphere of radius $r$ centred at $bold(c)$ maps to a sphere of radius $s r$ centred at $bold(A) bold(c) + bold(t)$. Under nonuniform scale the true image is an ellipsoid; the sphere is its conservative enclosure.

= Projection and depth

== Conventional finite-far matrix, for comparison

The camera looks along negative eye-space $z$, and positive axial depth is $d = -z_"eye"$. With vertical field of view $theta$, aspect $a$, near plane $n$, far plane $F > n$, and $f = 1 slash tan(theta slash 2)$, the following matrix maps near to $0$ and far to $1$. It is the *conventional-Z comparison*, not the engine matrix:

$ bold(P) = mat(delim: "[",
  f slash a, 0, 0, 0;
  0, f, 0, 0;
  0, 0, F/(n - F), (n F)/(n - F);
  0, 0, -1, 0
) $

== Reversed Z with an infinite far plane

The engine uses the reversed finite-far matrix and its infinite-far limit:

$ bold(P)_"rev" = mat(delim: "[", f/a,0,0,0; 0,f,0,0; 0,0,n/(F-n),(n F)/(F-n); 0,0,-1,0) $

$ bold(P)_infinity = mat(delim: "[", f/a,0,0,0; 0,f,0,0; 0,0,0,n; 0,0,-1,0) $

The finite mapping is $z_"ndc" = n(F-d)/(d(F-n))$. Its infinite-far limit is

#result[
$ z_"ndc" = n / d, quad d = "eye-space distance along the view axis" $
]

so $d = n$ gives $1$, and $d -> infinity$ gives $0$. There is no far plane to choose.

== Why reversed Z

Two effects compound in the standard mapping and cancel in the reversed one.

+ *Perspective depth is hyperbolic.* $z_"ndc"$ is a function of $1 slash d$, so depth values bunch near the near plane and spread out far away. Most of the numeric range is spent on the first few percent of the view distance.

+ *Float precision is not uniform.* IEEE-754 has constant #emph[relative] precision, so absolute precision is finest near $0$ and coarsest near $1$. Between $0.5$ and $1.0$ there are only $2^23$ representable binary32 values; between $0$ and any small $epsilon$ there are vastly more.

In the standard mapping both effects put precision where it is not needed and starve the far field. Reversing the range aligns them: distant geometry maps towards $0$, where floating-point absolute spacing is finer. The result is near-uniform relative precision across the whole range — effectively free accuracy for a one-line change.

#sheet(
  (auto, 1fr),
  [Consequence], [Required everywhere],
  [Depth test], [Greater-than. Nearer means larger],
  [Depth clear], [$0$],
  [Depth pyramid], [Reduces with $min$, not $max$ (§11)],
  [Far plane], [Infinite is the natural choice, at no precision cost],
)

= The float-ordering trick behind the visibility key

The 64-bit visibility record packs depth in the high half and a primitive token in the low half, then relies on a single integer atomic maximum to perform the depth test. That works because of the following.

#result[
For finite non-negative IEEE-754 binary32 values, with every zero canonicalized to $+0$, reinterpreting the bit pattern as an unsigned integer is a strictly monotonic map:
$ 0 <= x < y quad ==> quad "bits"(x) < "bits"(y) $
]

== Why it holds

A binary32 is laid out as sign (1 bit), exponent (8 bits), mantissa (23 bits), in that order from the most significant bit. For non-negative values the sign bit is $0$, so integer ordering is determined first by the exponent field and then by the mantissa field. The exponent is stored biased, so a larger exponent is a larger unsigned integer; within one exponent the mantissa is a fixed-point fraction increasing with the value. The property does not extend to negatives, because $+0$ and $-0$ have different bit patterns and the sign bit inverts the ordering — which is why the statement is restricted to non-negative values.

Reject NaNs, infinities and out-of-range values before packing. Canonicalize $-0$ to $+0$; the numerical test $z >= 0$ alone does not reject negative zero. Valid reversed-Z depth then qualifies.

== Packing

$ "key" = ("bits"(z) << 32) or "token" $

#sheet(
  (auto, 1fr),
  [Property], [Consequence],
  [$"token" = 0$ reserved], [A cleared buffer of zero reads as depth $0$ (far, under reversed Z) with no primitive],
  [Atomic $max$ on the key], [Selects the largest depth, which under reversed Z is the nearest surface],
  [Equal depths], [Fall through to comparing tokens, giving a deterministic order-independent tie-break],
)

== Why two 32-bit atomics is not equivalent

An atomic maximum on depth followed by a separate store of the token is two operations. Between them another thread can win the depth slot:

#align(center)[#sheet(
  (auto, auto),
  [Step], [Effect],
  [A: atomic depth update], [A wins at $d_A$; its token write is delayed],
  [B: atomic depth update], [B wins at $d_B > d_A$],
  [B: token write], [The pixel temporarily agrees on B],
  [A: delayed token write], [Depth is B's; token is A's],
)]


#warn[
The pixel ends up with depth from one triangle and identity from another. It shades the wrong surface, intermittently, at a rate depending on GPU occupancy — which can make local reproduction difficult. The 64-bit width is load-bearing, not a convenience.
]

== Token budget

The low 32 bits index instance, cluster and local triangle. At 128 triangles per cluster the triangle index takes 7 bits, leaving 25 bits for the cluster-instance pair:

$ 2^25 = 33\,554\,432 quad "distinct visible cluster instances per frame" $

Use a frame-local lookup from cluster-instance index to stable instance and cluster identity. Reserving index $0$ leaves $2^25-1$ complete 128-triangle blocks; do not also claim all $2^25$ blocks while reserving token zero. Check capacity before emission. A wrapped token aliases two surfaces. Ordinary hardware depth testing does not reproduce the token tie-break automatically: match it explicitly or classify equal-depth ties separately.

= Screen-space error

This is the single most important equation in the runtime. It converts an object-space geometric error into pixels, so the selector can compare it against a threshold.

== Derivation

At eye-space depth $d$, the vertical extent of the view frustum is $2 d tan(theta slash 2)$. With a viewport of $H$ pixels vertically, one world unit at depth $d$ therefore covers

$ rho(d) = H / (2 d tan(theta slash 2)) quad "pixels per world unit" $

An object-space error $e_"obj"$ stretched by the instance transform becomes a world-space error $s dot e_"obj"$, giving

#result[
$ e_"px" approx (e_"obj" dot s dot H) / (2 tan(theta slash 2) dot d_"near") $
]

== Using it conservatively

The nearest point of the bounding volume is what enters the denominator, not the centre. For a sphere of centre $bold(c)$ and radius $r$, with near plane $n$:

$ d_"near" = max(n, hat(bold(f)) dot (bold(c) - bold(e)) - r) $

Here $hat(bold(f))$ is the unit world-space camera-forward vector and $r$ is the world-space radius. Radial distance $norm(bold(c)-bold(e))$ is generally larger than axial depth and can understate error away from the view axis. Clamping prevents division by zero; it does not prove a bound for near-plane crossings or arbitrary 3-D displacement.

=== Monotonicity matters more than accuracy

For a tree, or a group-coherent replacement relation with explicit boundary cases, the selector's local predicate is

$ "draw"(v) quad <==> quad e_"px" ("parent"(v)) > tau quad and quad e_"px" (v) <= tau $

This predicate alone does not define a legal general DAG cut; §16 supplies the replacement and residency conditions. If $e_"px"$ is not monotonic along a dependency path, both conditions can hold for two nodes covering the same surface region, or for neither — producing a double-draw or a hole. This is why the gate validates monotonicity of the #emph[projected] error rather than the scalar object-space error: projection involves $s$ and $d_"near"$, either of which can reorder two nodes whose object-space errors are correctly ordered.

=== Near-plane crossings

When the bounding volume straddles the near plane, $d_"near"$ clamps to $n$ and the closed form saturates. Force refinement, or use a conservative projection that accounts for the clipped volume. The approximation must not be trusted there.

=== Orthographic views

No depth dependence at all:

$ e_"px" = (e_"obj" dot s dot H) / h_"ortho" $

Shadow cascades are orthographic, which is why they need their own error metric expressed in shadow-map texels rather than in screen pixels.

== Inverting it

The distance at which a node with error $e_"obj"$ first satisfies a threshold $tau$:

$ d_"switch" = (e_"obj" dot s dot H) / (2 tan(theta slash 2) dot tau) $

At $1080"p"$, $theta = 70 degree$, $tau = 1$ px, $s = 1$:

$ d_"switch" = (1080 dot e_"obj") / (2 dot 0.7002) approx 771 dot e_"obj" $

The paraxial estimate for $1$ cm of error crosses one pixel at roughly $7.7$ m of nearest axial depth. This is a sanity check, not a certified switching distance for arbitrary off-axis displacement.

== A bound for finite three-dimensional displacement

Write the pixel projection as $pi(x,y,d) = (k_x x/d, k_y y/d)$ up to an origin offset, with $k_y = H/(2 tan(theta/2))$ and $k_x = W f/(2a)$. Its Jacobian is

$ bold(J) = mat(k_x/d, 0, -k_x x/d^2; 0, k_y/d, -k_y y/d^2) $

For square pixels $k_x = k_y = k$, its operator norm is $k sqrt(1+(x^2+y^2)/d^2)/d$. The off-axis factor is the term the simple estimate omits.

Suppose every line segment joining corresponding original and simplified points lies in a domain with $d >= d_min > 0$ and $sqrt(x^2+y^2)/d <= q_max$. Integrating the Jacobian along each segment gives

#result[
$ e_"px" <= K s e_"obj", quad K = (max(k_x,k_y)/d_min) sqrt(1+q_max^2) $
]

For a view-space ball with centre $(c_x,c_y,d_c)$ and radius $R$, a conservative choice is $d_min=d_c-R$ and $q_max=(sqrt(c_x^2+c_y^2)+R)/d_min$. Enclose both surfaces and their connecting segments, including quantization and deformation uncertainty. Use this bound only while the domain lies wholly beyond the near plane. Otherwise refine or report an explicit finest/resident fallback; do not claim clipping is covered by this derivation.

This bounds projected surface displacement. It does not by itself bound visibility changes through thin occluders, texture discontinuities or every silhouette correspondence. Q2 remains a separate image-space test.

= Bounding volumes

== Minimal enclosing sphere

#sheet(
  (auto, auto, 1fr),
  [Algorithm], [Cost], [Quality],
  [Ritter], [$O(n)$, two passes], [Typically $5$–$20%$ larger than minimal],
  [Welzl], [Expected $O(n)$, randomized], [Exact minimal sphere],
)

Welzl's method rests on the fact that the minimal enclosing sphere of a point set in $RR^3$ is determined by at most four boundary points. The supplied benchmark gates do not specify a 10% sphere-radius requirement. Measure bound tightness before adopting one; neither the quoted typical Ritter range nor floating-point Welzl output is a universal accuracy guarantee. Always verify containment and inflate for numerical error.

A loose sphere is not a correctness bug but it is a performance one: it lowers $d_"near"$, inflating $e_"px"$ and forcing refinement that buys nothing.

== Conservative screen projection of a sphere

Needed for the Hi-Z footprint.

#warn[
Projecting the centre, projecting $bold(c) plus.minus r$ along the axes and taking the bounding box *under-covers*. An under-covering footprint causes false occlusion, which is a visible hole.
]

The perspective image of a sphere is not a circle but a conic. Its screen-space bounding rectangle comes from the cone tangent to the sphere from the eye. Working in the $x z$ plane, a line through the origin with direction $hat(bold(u)) = (sin phi, cos phi)$ is tangent to the circle of centre $bold(c)$ and radius $r$ when

$ norm(bold(c) times hat(bold(u))) = r $

which is a quadratic in the tangent direction with two roots, giving the horizontal extent. The same construction in the $y z$ plane gives the vertical extent.

Degenerate cases must be handled explicitly rather than discovered: if the eye lies inside the sphere the footprint is the whole screen, and if the sphere straddles the near plane, treating it as visible is a safe baseline. A sphere clipped to $d >= n > 0$ has finite projected bounds; near-plane intersection alone does not make them unbounded. The closed form, with the clipping cases worked out, is in Mara and McGuire — use it rather than rederiving it, because the sign cases are where the bugs live.

== Normal cones

Let outward face normals lie within half-angle $alpha$ of unit axis $hat(bold(a))$. Let $hat(bold(v))$ point from the sphere centre to the eye, and let $D=norm(bold(c)-bold(e)) > r$. View directions across the sphere differ from $hat(bold(v))$ by at most $beta=arcsin(r/D)$.

Every face is backfacing if its outward normal has a negative dot product with its direction to the eye. A sufficient rejection condition is

#result[
$ alpha+beta < pi/2 quad and quad hat(bold(a)) dot hat(bold(v)) < -sin(alpha+beta) $
]

Indeed, the angle between the axis and centre-to-eye direction then exceeds $pi/2+alpha+beta$. Subtracting both angular spreads still leaves every normal more than $90 degree$ from its viewing direction. For a planar point cluster this reduces to $hat(bold(a)) dot hat(bold(v)) < 0$, the necessary sign sanity check.

Choose a nonzero candidate axis and compute $alpha=max_i arccos("clamp"(hat(bold(a)) dot hat(bold(n))_i,-1,1))$. A vanishing normal sum, $D <= r$, or a cone too broad for the condition means no rejection. Expand the cone for encoding error. This angle convention is not meshoptimizer's stored `cone_cutoff`; use its documented apex/cutoff test unchanged when consuming those fields.

#warn[
Disable cone culling for two-sided or deformed geometry. Under a general affine instance transform, rotating the cone axis while leaving its aperture unchanged is unsafe. A convenient static-mesh alternative is to transform the eye into object space and use the original cone there, with mirrored winding handled consistently. Singular transforms cannot use that inverse and need an explicit reject-or-fallback policy.
]


= Quadric error metrics

The simplifier's core ordering metric. It does not by itself certify the geometric error stored in the LOD hierarchy; §8 distinguishes those quantities.

== The fundamental quadric

A plane is $bold(p) = (a, b, c, d)^T$ with $a^2 + b^2 + c^2 = 1$. The squared distance from a homogeneous point $tilde(bold(v))$ to that plane is

$ D^2 (tilde(bold(v)), bold(p)) = (bold(p) dot tilde(bold(v)))^2 = tilde(bold(v))^T (bold(p) bold(p)^T) tilde(bold(v)) $

which defines the fundamental error quadric as a $4 times 4$ symmetric outer product:

#result[
$ bold(K)_bold(p) = bold(p) bold(p)^T $
]

It has $10$ distinct entries, which is how it is stored.

== Accumulation

For a vertex incident to a set of planes, the weighted sum of squared distances is

$ bold(Q)(v) = sum_(bold(p) in "planes"(v)) w_bold(p) bold(K)_bold(p), quad "typically" w_bold(p) = "face area" $

and the error of placing a vertex at $tilde(bold(v))$ is the quadratic form

$ Delta(tilde(bold(v))) = tilde(bold(v))^T bold(Q) tilde(bold(v)) $

Because quadrics are additive, contracting an edge $(v_1, v_2)$ to a new vertex $macron(v)$ gives

$ bold(Q)(macron(v)) = bold(Q)(v_1) + bold(Q)(v_2) $

This additivity is the whole reason the method scales: error accumulates through arbitrarily many contractions at constant cost per vertex.

== Optimal contraction position

Minimizing $Delta(tilde(bold(v)))$ with the homogeneous coordinate fixed at $1$, and setting the partials with respect to $x, y, z$ to zero, gives a $3 times 3$ linear system in the upper-left block of $bold(Q)$:

$ mat(delim: "[",
  q_11, q_12, q_13;
  q_12, q_22, q_23;
  q_13, q_23, q_33
) vec(x, y, z) = vec(-q_14, -q_24, -q_34) $

#warn[
The matrix is singular whenever the incident planes fail to span three independent directions — a flat region, a straight edge, a symmetric configuration. *This is the common case, not an edge case.* Fall back in order: minimize along the edge segment, then choose whichever of $v_1$, $v_2$ or the midpoint gives the lowest error.
]

== Attribute-aware simplification

Position-only quadrics will happily collapse a UV seam or a hard normal crease, producing geometry that is correct and shading that is wrong. The extension lifts the problem into a higher-dimensional space where a vertex carries $(x, y, z, u, v, dots.h.c)$ and the quadric is built there, so attribute discontinuity contributes to the error.

Two practical consequences. Attribute weights are a tuning parameter with no universally right value, trading silhouette accuracy against shading accuracy. And attribute discontinuities fragment the mesh into components the simplifier cannot merge across, which is why heavily faceted meshes resist simplification.

== Locking boundaries

Locking a vertex is not a modification to the quadric. It is a constraint on the contraction set: an edge with a locked endpoint is either not contractible, or contractible only to the locked position. This is what keeps a group's boundary bit-identical before and after simplification, which is what makes a cut crack-free.

#warn[
Positional locking does not lock UVs. Two clusters can agree exactly on boundary positions while their interior parameterization drifts, giving texture swimming across LOD transitions on a mesh with no geometric cracks at all.
]

= Error bounds and monotonicity

== Quadric error is not a distance

$Delta(tilde(bold(v)))$ is a weighted sum of squared distances to the #emph[original planes]. It is a useful ordering heuristic and a poor bound — it can be small while the surface has moved a long way, in regions where the incident planes are nearly parallel.

What the runtime needs is a conservative bound on geometric deviation, because the cut predicate compares it against a pixel threshold and a claim of "under one pixel" has to be true.

== Hausdorff distance

The honest measure. For surfaces $S$ and $S'$:

$ d(S, S') = sup_(bold(x) in S) inf_(bold(y) in S') norm(bold(x) - bold(y)) $

$ d_H (S, S') = max(d(S, S'), d(S', S)) $

The two-sided form is the one that matters: a one-sided distance can be zero while the simplified surface has grown a spurious sheet.

Exact Hausdorff distance between triangle meshes is expensive. Dense sampling alone gives a #emph[lower] bound; an arbitrary percentage margin does not turn it into an upper bound. Use certified distance bounds or sampling with a certified covering radius.

For samples $X subset S$, suppose every point of $S$ is within $h$ of some sample, and each computed distance $u_x$ bounds $d(x,S')$ from above. The distance-to-set function is 1-Lipschitz, so

#result[
$ d(S,S') <= max_(x in X) u_x + h $
]

Repeat in the reverse direction and take the maximum. A verified subdivision covering each triangle by sample balls supplies $h$; sample density without that coverage proof does not. Account for distance-query roundoff. This is an offline validator, not a recommendation to add an expensive mesh-distance pass to every production cook before measuring its cost.

== Monotonicity up the DAG

The invariant, for every edge from child to parent, and after projection for every dependency path and every camera:

#result[
$ e("child") <= e("parent") $
]

Two ways it is violated in practice.

*Construction.* A parent simplified from an unusually cooperative group ends up with lower measured error than one of its children. First accumulate actual approximation error. If the parent approximates the union of its children with certified error $delta_p$, the Hausdorff triangle inequality gives

$ e_p <= delta_p + max_c e_c $

when each child bound is relative to its corresponding source region. Store the right-hand side as a conservative envelope, or use a direct certified parent-to-source bound. Quantization is included once at each newly decoded representation. Taking a maximum of child errors alone does not account for new simplification displacement.

Then enforce monotonicity, without mistaking it for error accumulation, by forcing

$ e("parent") := max(e("parent"), max_(c in "children") e(c) + epsilon) $

*Projection.* Two nodes with correctly ordered object-space errors can invert after being scaled by $s$ and divided by $d_"near"$, if their bounding volumes differ enough. This is why validating projected monotonicity under nonuniform transforms and near-plane cases is a gate, and why scalar monotonicity alone does not pass.

Quantization error (§12) is part of the same envelope and must be folded in before the bound is stored, not carried as a separate budget.

A sufficient projection construction is to nest the convex error domains: each parent domain contains every child domain, while $e_p >= e_c$. Let $K_g$ be the supremum projection-Jacobian norm on that domain. Then $K_p >= K_c$ and $K_p s e_p >= K_c s e_c$. If using looser analytic estimates whose ordering is not automatic, explicitly propagate $K_p := max(K_p,max_c K_c)$, or propagate projected errors in a reference pass. Mere camera sampling is evidence, not an all-camera proof. Group decision bounds and tight culling bounds may be separate.

= Graph partitioning

Grouping clusters for simplification is balanced graph partitioning. Build $G = (V, E)$ with clusters as vertices and edge weight $w(u, v)$ equal to the number of boundary edges shared. Grouping into sets of four to eight means finding a $k$-way partition minimizing

$ "cut"(P) = sum_((u,v) in E : P(u) != P(v)) w(u, v) $

subject to a balance constraint

$ |P_i| <= (1 + epsilon) |V| / k $

== Why minimize the cut

The cut edges are exactly the boundary that gets locked during simplification. A small cut locks fewer edges, so the simplifier has more freedom and achieves better reduction. A large cut produces groups that cannot be simplified — which is the failure mode the whole regrouping scheme exists to avoid.

== Solving it

The problem is NP-hard. Practical solvers are #emph[multilevel]: coarsen the graph by repeatedly contracting matched edges, partition the small coarse graph directly, then uncoarsen while refining at each level with a local heuristic. Kernighan–Lin is the classical refinement; Fiduccia–Mattheyses is a common refinement method with a bucket structure. Inspect the pinned partitioner rather than assuming it uses this exact method.

#warn[
Disconnected geometry breaks the assumption. If a group's clusters share no edges, the cut is zero regardless of partition and the grouping is arbitrary. Spatial proximity has to substitute for topological adjacency there.
]

= Rasterization

Required to understand the hardware path, and required in full if a compute rasterizer is ever built.

== Edge functions

For a directed edge from $(x_0, y_0)$ to $(x_1, y_1)$:

#result[
$ E(x, y) = (x - x_0)(y_1 - y_0) - (y - y_0)(x_1 - x_0) $
]

This is the 2-D cross product of the vector to the sample point with the edge vector, the negative of the opposite ordering. It is positive on one side, negative on the other, zero on the line, and a point is inside the triangle when all three edge functions agree in sign.

$E$ is affine in $x$ and $y$, so stepping one pixel in $x$ adds $(y_1 - y_0)$ and stepping in $y$ subtracts $(x_1 - x_0)$. Incremental evaluation is the basis of every scanline and tiled rasterizer.

== Barycentrics

The edge functions #emph[are] the unnormalized barycentric coordinates:

$ lambda_0 = E_(1 2) / A, quad lambda_1 = E_(2 0) / A, quad lambda_2 = E_(0 1) / A $

with $A = E_(0 1)(x_2, y_2)$, twice the signed triangle area.

== Perspective-correct interpolation

Screen-space linear interpolation of an attribute is wrong under perspective. The quantity that is screen-affine is the attribute divided by $w$:

#result[
$ a(x, y) = (sum_i lambda_i dot a_i / w_i) / (sum_i lambda_i dot 1 / w_i) $
]

Depth is the exception. $z_"ndc"$ is already the result of a perspective divide, so it interpolates linearly in screen space directly. Interpolating eye-space $z$ linearly is the classic bug.

== Fixed point and watertightness

Vertices are snapped to a fixed-point grid, commonly 8 sub-pixel bits giving $1 slash 256$ px. This is not an optimization; it is what makes rasterization watertight.

With floating-point positions, the edge function evaluated for two triangles sharing an edge can disagree in sign for a sample near that edge, because the two evaluations use different operand orders and round differently. The result is a pixel hit twice, or not at all. Snapping to a shared integer grid makes the edge function an exact integer computation, so both triangles compute identical values along the shared edge and the classification is consistent by construction.

The integer range must be chosen so the edge function cannot overflow: with coordinates in $[0, 2^k)$ sub-pixel units, $E$ needs about $2k + 2$ bits.

== Fill rule

Exact coverage still leaves samples lying exactly on an edge, which both triangles would claim. The top-left rule breaks the tie. For this table use framebuffer coordinates with positive $y$ downwards and orient the triangle so interior edge functions are positive ($A>0$). A sample on an edge belongs to the triangle when that edge is a top edge or a left edge. Backends with a different viewport orientation need the corresponding sign conversion.

#sheet(
  (auto, 1fr),
  [Edge], [Definition],
  [Top], [Horizontal, with the triangle's interior below it],
  [Left], [Not horizontal, and going downwards in the winding direction],
)

A compute rasterizer must match the hardware rule exactly, or the two paths disagree along their shared boundary. That is the seam-correctness gate.

== Quad overshading

Hardware rasterizes fragments in $2 times 2$ quads so it can compute derivatives by finite differencing. Derivative-dependent shading may execute helper lanes for uncovered samples; the precise invocation and execution cost depends on the shader and GPU. For a triangle of screen area $A$ pixels, efficiency is

$ eta = A / (A + "perimeter cost"(A)) $

As $A -> 0$ the perimeter term dominates, $eta$ falls below $1 slash 4$, and setup cost is paid regardless. This is a workload model, not an exact invocation-count formula. Setup, helper-lane cost, depth rejection and atomic contention all matter. A visibility-only shader may use no derivatives at all; measure its actual instructions and throughput. Tiny rejected triangles can consume setup work while contributing zero covered pixels.

= Hierarchical depth and occlusion

== The pyramid

Build a mip chain over the depth buffer. Under reversed Z a conservative occluder depth is the farthest, which is the minimum:

#result[
$ "mip"[L][i] = min("four texels of mip"[L-1]) $
]

#warn[
Using $max$ here — the habit from standard-Z pipelines — produces a pyramid reporting surfaces as nearer than they are, which rejects visible geometry. Holes.
]

Uncovered pixels retain the clear value $0$, the farthest possible depth, so nothing can be occluded through a hole.

== Level selection

For a screen-space footprint of width $w$ and height $h$ pixels, choose

$ L = "clamp"(ceil(log_2 (max(1,w,h))),0,L_max) $

so the footprint spans at most $2 times 2$ texels at level $L$ and is tested with at most four fetches. Too coarse is conservative when the reduction covers all source samples. A finer level is also correct if every overlapped texel is fetched; the failure is fetching only four while omitting part of the footprint. Use outward-rounded integer pixel bounds, nearest texel loads, and explicit odd-dimension handling. A padded power-of-two pyramid with zero-valued padding is a simple conservative construction. When a capped level spans more than two texels, fetch the entire footprint.

== The test

A cluster is occluded when its nearest possible depth lies behind the farthest occluder across its entire footprint:

$ "occluded" quad <==> quad z_"near"("cluster") < min_"footprint" "HZB"[L] $

For reversed Z, use an *upper* bound on candidate nearest depth and a *lower* bound on occluder depth. A safe numerical test is

$ z_"candidate"^+ + epsilon_z < z_"occluder"^- $

with $epsilon_z >= 0$. Overestimating candidate depth or underestimating the pyramid reduces culling and is safe. *Underestimating the candidate or overestimating the pyramid causes false rejection.* With infinite reversed Z, a sphere wholly beyond the near plane has $z_"candidate"^+=n/(d_c-r)$ before outward numerical rounding.

== Two passes

Pass one tests against the previous frame's pyramid using #emph[previous] transforms — valid only as a *provisional* rejection, because it asks whether the cluster was hidden last frame. Pass two rebuilds the pyramid from pass-one depth and re-tests the rejected set with #emph[current] transforms, recovering everything that became visible this frame. The asymmetry is what makes the scheme correct rather than merely cheap.

= Quantization and numerical precision

== Grid quantization

Positions are stored on a shared object-space lattice. For per-axis extents $L_i$ and bit counts $b_i$, nearest rounding gives

$ Delta_i = L_i/(2^(b_i)-1), quad |epsilon_i| <= Delta_i/2 $

#result[
$ e_q <= (1/2) sqrt(Delta_x^2+Delta_y^2+Delta_z^2) $
]

Zero-extent axes decode to their fixed coordinate. For equal $b$ and per-axis steps derived from the extents, with diagonal $D=sqrt(L_x^2+L_y^2+L_z^2)$:

$ e_q <= D/(2(2^b-1)) <= 10^(-5)D quad ==> quad b >= 16 $

This holds for every aspect ratio; 16 bits does not become insufficient merely because the box is thin. A thin feature may need a tighter *feature-relative* target. If the format instead uses one isotropic step based on $L_max$, use $e_q <= sqrt(3)L_max/(2(2^b-1))$; that distinct convention can require 17 bits for the same diagonal-relative target. Include floating-point decode error in addition to these ideal rounding bounds.


== Seam vertices share the grid

Two clusters meeting at a seam store the same vertex twice. If each quantizes against its own local bounding box, the copies decode to different positions and the seam cracks by up to one grid step. A shared object-space grid, identical integer coordinates and the same decode operations make both decodes bit-identical within a backend — a stronger and cheaper guarantee than any tolerance.

Quantization error is part of the LOD error envelope. The stored per-node bound must include it, or the runtime's claim of sub-pixel error is false by exactly this amount.

== Floating-point determinism

Cooking is Linux-only precisely because bit-identical results across architectures are not achievable without effort disproportionate to the benefit.

#sheet(
  (auto, 1fr),
  [Source of divergence], [Mechanism],
  [FMA contraction], [$a b + c$ may compile to one fused operation with one rounding instead of two. AArch64 contracts aggressively by default],
  [Reassociation], [$(a + b) + c != a + (b + c)$ in floating point, and auto-vectorization reassociates reductions],
  [Library functions], [$sin$, $exp$, $"pow"$ are not specified to the last bit by IEEE-754; implementations differ in the final ULP],
  [Extended precision], [Historic x87 intermediates at 80 bits, and other width mismatches],
)

Quadric solves and spatial partitioning amplify tiny differences into different contraction orders and therefore different meshes. Within one toolchain on one architecture, determinism is achievable and is a gate.

= Sizing arithmetic

== Cluster and DAG counts

For $T$ source triangles at $t$ triangles per cluster, the leaf count is $N = T slash t$. At $T = 10^8$ and $t = 128$ that is about $781\,000$ leaf clusters.

If each level reduces triangle count by a factor $rho$, the total node count across all levels is a geometric series:

#result[
$ "total" = N sum_(k=0)^infinity rho^k = N / (1 - rho) $
]

#sheet(
  (auto, auto, 1fr),
  [$rho$], [Total], [Reading],
  [$0.4$], [$1.67 N$], [Aggressive reduction, fewer levels],
  [$0.5$], [$2 N$], [The target. The whole hierarchy costs twice the leaf level],
  [$0.6$], [$2.5 N$], [Timid reduction, more levels and more storage],
)

Sensitivity to $rho$ is mild, which is why a $40%$ reduction target is adequate and why chasing a higher ratio buys little storage while costing quality.

Levels until a root set of $R$ clusters:

$ "levels" = log(N slash R) / log(1 slash rho) $

From $781\,000$ leaves to about $32$ roots at $rho = 1 slash 2$ gives $14.6$, so fifteen levels.

== Bytes

$ "cooked bytes" approx T / (1 - rho) dot beta, quad beta = "bytes per triangle" $

At $T = 10^8$, $rho = 1 slash 2$, $beta = 16$ B: about $3.0$ GiB.

#warn[
Dataset size is a #emph[consequence] of triangle count and encoding efficiency, not an independent target. Set the pool from the oversubscription ratio the proof requires:
$ R_"disk" = B_"disk" / B_"pool", quad R_"resident" = B_"decoded unique pages" / B_"usable" $
]

Report both ratios. Disk compression, padding and metadata make them different. Real residency pressure is measured in the format occupying physical slots, plus charged metadata; repeated loads of the same page do not increase the unique working set. At 16 bytes per stored triangle and a twofold hierarchy, an 8 GiB disk target needs about 268.4 M source triangles before overhead. 100 M triangles alone does not imply 8 GiB.

== Slots

$ "slots" = "pool bytes" / "page bytes" $

One GiB at $128$ KiB per page gives $8\,192$ slots, less reservations for pinned roots, BVH and DAG metadata, and the page table itself. Those are charged against the same budget, so usable slot count is always below the nominal division.

== Screen size is an empirical scaling test

At $1920 times 1080$ there are $2\,073\,600$ pixels, but a 1 px geometric error bound implies no universal triangle count. A flat rectangle may be represented exactly by two triangles; many thin disconnected surfaces or overlapping layers may require millions. An error threshold bounds deviation, not projected triangle area, depth complexity, topology or material fragmentation.

Treat 0.5–2 M main-view triangles and 1–3 M including cascades as provisional corpus-specific expectations, not lower bounds, upper bounds or correctness gates. The useful experiment is controlled 1×/2×/4× tessellation of the same surfaces, with fixed camera, error and residency. Measure traversal as well as selected triangles. Shadow work additionally depends on cascade extents and caster coverage.


== Bandwidth

Per full-screen pass at $1080"p"$:

$ "bytes" = 2\,073\,600 dot beta_"px" dot ("reads" + "writes") $

A 64-bit visibility buffer read-modify-written is $2.07 "M" times 8 times 2 approx 33$ MB. On a device with about $250$ GB/s that is roughly $0.13$ ms of pure bandwidth, before latency and occupancy effects. Small against a $16.7$ ms budget, less small against $4$ ms, and doubled by the two-pass occlusion scheme.

= Queueing: why latency shows up as depth

The streaming path is a queueing system, and Little's law describes it exactly.

#result[
$ L = lambda W $
]

$L$ is the mean number of items in the system, $lambda$ the mean arrival rate, $W$ the mean time in the system. It holds for any stable system regardless of arrival distribution or service discipline.

Applied to page loading: at $7$ pages per tick with $10$ ticks of latency, there are $70$ pages in flight at steady state. Three consequences.

+ *With sufficient concurrency and unchanged service capacity*, added latency increases in-flight depth at a fixed throughput. With finite concurrency $C$, throughput is also bounded by $C/W$. This is an assumption to test, not a promised shape of the throttled-I/O result. Staging must cover occupied bytes across the full pipeline.

+ *Under-sizing the staging ring converts a latency problem into a throughput problem*, because admission stalls waiting for a staging slot. That failure is easy to misdiagnose as slow storage.

+ *Time to recover quality after a teleport* depends on feedback, dependency rounds, reads, decode, upload, publication and the confirmation window. For pipelined independent pages a useful estimate is latency plus deficit divided by bottleneck throughput; it is not a guaranteed deadline. At 128 KiB and 256 MiB/s, the read ceiling is 2,048 pages/s. Covering 10 ms of read latency needs at least 21 outstanding pages before other delays. A 400 MiB read takes at least 1.5625 s at that rate.

The 30-frame quality confirmation adds $30/f$ seconds if included in reported recovery: 0.5 s at 60 Hz and 1 s at 30 Hz. Define whether the timestamp means first qualifying frame or completion of confirmation. Multi-page activation requires enough space for the old fallback and the newly staged closure simultaneously; see §17.

= Statistics for the benchmark suite

== Never average frame rates

Frame rate is a rate; frame time is a duration. Averaging rates weights fast frames more heavily and hides stutter. Two frames at $120$ fps and one at $20$ fps:

#sheet(
  (auto, auto, 1fr),
  [Method], [Result], [Verdict],
  [Mean of rates], [$(120 + 120 + 20) slash 3 = 86.7$ fps], [Wrong, and flattering],
  [Mean of times], [$(8.33 + 8.33 + 50) slash 3 = 22.2$ ms $-> 45$ fps], [Correct],
)

The correct average of rates is the harmonic mean, which equals the reciprocal of the mean frame time:

$ macron(f) = n / (sum_i 1 / f_i) = 1 / macron(t) $

Work in the time domain and convert once at the end, if at all.

== Percentiles

A gate of $p_95 = 16.667$ ms means $95%$ of frames completed within the $60$ fps budget. It does not mean the frame rate was $60$. Percentiles are computed on individual frame samples within one run; run-level statistics are then compared across runs.

Estimating a $p$ quantile stably needs roughly $10 slash (1 - p)$ samples as a bare minimum and $100 slash (1 - p)$ for a tight interval:

#sheet(
  (auto, auto, 1fr),
  [Quantile], [Samples needed], [At 60 fps],
  [$p_95$], [$200$ – $2\,000$], [3 s – 33 s],
  [$p_99$], [$1\,000$ – $10\,000$], [17 s – 2.8 min],
  [$p_99.9$], [$10\,000$ – $100\,000$], [2.8 min – 28 min],
)

A $120$-second route at $60$ fps is $7\,200$ frames: useful for $p_99$, with only about seven observations in the upper 0.1%. These counts are heuristics, not confidence guarantees; frame autocorrelation reduces information. The supplied benchmark file has no explicit $p_99.9$ gate. Do not silently introduce one.

== Run-to-run variation

$ "CV" = sigma / mu $

CV is not the same statistic as a median-relative variation rule. Make the existing rule explicit, for example $max_i |q_i-"median"(q)|/"median"(q) <= 0.10$ for the five run-level $p_95$ values $q_i$. A violation calls for diagnosis and a repeat; it does not prove which source of variation is responsible. This is a gate on the #emph[measurement], not on the engine.

== Regression detection

A difference is real when it exceeds noise #emph[and] repeats. Thresholds of $> 5%$ on $p_95$ and $> 10%$ on $p_99$, repeating across five runs, are chosen so that normal variation does not trip them. Auto-failing on a single $6%$ move produces noise, and noise produces disabled jobs.

== Image comparison

#sheet(
  (auto, 1fr),
  [Rule], [Reason],
  [MAE in linear RGB, not sRGB], [A fixed sRGB difference means different physical differences at different brightnesses],
  [Per-channel LSB cap alongside any mean], [A mean hides a small bright artifact: one pixel wrong by 200 levels out of two million moves MAE by $0.0001$],
  [Identity masks compare exactly], [On matched topology with stable IDs, outside the archived tie/edge band. Different LODs have different triangle identities; compare their geometry and coverage instead],
)

= Legal cuts and bounded GPU work

The replacement DAG describes compatible changes of representation. The culling BVH only finds candidates. Neither a per-cluster residency bit nor a tree predicate is a substitute for a legal cut.

== Coverage and replacement

Let $Omega$ be the abstract source domain, partitioned into ownership regions for validation. These regions express provenance and replacement compatibility, not an assertion that a simplified triangle lies on an original triangle. A selected representation $C$ is covering when

#result[
$ sum_(v in C) chi_(Omega_v)(x) = 1 quad "for every source region" x $
]

Use this equation only where the format actually supplies compatible ownership regions. Regrouping can move internal boundaries; do not invent a permanent source-triangle-to-cluster mapping for each coarse cluster. The general validator should instead replay certified replacement operations. For a replacement group $g$, let $C_g$ be its coarse side and $F_g$ its fine side. Both sides represent the same source domain with compatible external boundary, even though their geometric surfaces differ within the stored error bound.

$ C' = (C without C_g) union F_g $

This transition is legal only if the full coarse side is present, the fine side and its dependencies are available, and the operation is compatible with every overlapping replacement already active. Prove coverage at the pinned roots, then preserve it inductively through legal transitions. Count selected cluster-instance IDs once; BVH reachability through two paths is not permission to emit twice. Validate coverage before visibility culling, since culled surfaces intentionally disappear from raster submission.

== Coherent hysteresis

Let $E_g$ be the projected error of the coarse representation and $tau_"coarsen" < tau_"refine"$. For a strict requested maximum $tau$, choose $tau_"refine" <= tau$. Otherwise the hysteresis band itself permits exceeding the requested error and must be reported as such.

#sheet(
  (auto, 1fr),
  [State], [Transition],
  [Coarse], [Refine when $E_g > tau_"refine"$ and the entire compatible fine closure can commit],
  [Fine], [Coarsen when $E_g < tau_"coarsen"$ and the compatible coarse closure is resident],
  [Within the band], [Keep the coherent group state],
  [Blocked refinement], [Keep a legal covering cut; mark requested-error fallback],
)

All group members share the state. Roots, leaves, invalid history and irreducible geometry have explicit boundary cases. A finest-level quantized representation can still exceed the target close to the camera; leaves are not automatically zero-error geometry.

== Frustum rejection and affine boxes

For an inward plane $bold(n) dot bold(x)+b >= 0$, reject a sphere only if

$ bold(n) dot bold(c)+b+r norm(bold(n)) < 0 $

For an axis-aligned box with centre $bold(c)$ and half-extents $bold(h)$, replace $r norm(bold(n))$ with $|bold(n)| dot bold(h)$. Absolute values are componentwise. Under affine $bold(A),bold(t)$, the enclosing transformed AABB is

$ bold(c)'=bold(A)bold(c)+bold(t), quad bold(h)'=|bold(A)|bold(h) $

For world-to-clip matrix rows $bold(r)_0,dots.h.c,bold(r)_3$, clip inequalities give planes $bold(r)_3 plus.minus bold(r)_0$, $bold(r)_3 plus.minus bold(r)_1$, $bold(r)_2$, and $bold(r)_3-bold(r)_2$. With reversed Z, the physical near plane is the last one. Infinite-far projection makes the lower-depth plane degenerate: omit it instead of normalizing a zero normal. Include declared quantization and deformation expansion before rejection.

== Compaction and capacity

For keep flags $b_i in {0,1}$, an exclusive prefix sum assigns unique output offsets:

$ o_i=sum_(j<i)b_j, quad N_"out"=sum_i b_i $

Emit item $i$ at $o_i$ when $b_i=1$. For group sizes $k_g$, reserve the complete interval from an exclusive scan of $k_g$, and commit only if its end is inside capacity. A truncated group violates coverage. A safe baseline first counts the desired output; if it cannot fit, select a complete coarser cut that has its own reserved capacity. Do not suppress parents before successful child commitment.

If a level has at most $N$ inputs and fan-out at most $b$, its unculled next-level candidate bound is $b N$, not the final visible count. Persistent queues additionally require a proof of forward progress; an atomic counter does not guarantee that a producer workgroup can be scheduled while consumers spin.

Source map: Nanite and cluster hierarchy references (§§1, 3 of references); Blelloch and meshoptimizer (§5); synchronization specifications (§9).

= Page activation, publication and lifetime

== Dependency closure and simultaneous space

Let $D(p)$ name page $p$'s dependencies and $P(g)$ the pages containing a replacement group. Define the least transitive closure

$ "cl"(S)=S union union_(p in S) D(p) union union_(q in union_(p in S)D(p))D(q) union dots.h.c $

The format must make this closure finite. Collapse mutual activation dependencies into one activation unit when necessary. For the immutable resident set $R_f$ of frame $f$:

#result[
$ "ready"(g,f) quad <==> quad "cl"(P(g)) subset.eq R_f $
]

Readiness is necessary, not sufficient: the legal-cut compatibility checks in §16 still apply. Metadata needed to resolve dependencies and choose a fallback must itself be reachable without fetching the missing page.

For pool cap $B$, page size $P$, and separately charged resident metadata $M$, the physical slot count is $floor((B-M)/P)$. Do not subtract metadata again if it is already in those pages. At every instant,

$ B_"pinned" + B_"active-only" + B_"staged-only" + B_"retiring-only" + M <= B $

The byte sets in this accounting are disjoint. A fine closure fitting alone is insufficient: it must coexist with its still-live fallback and unreclaimed readers. Admission checks the incremental closure cost, including dependency amplification and padding. If it cannot fit, keep the coarser cut and report infeasibility rather than retrying forever.

== Publication is an ordering relation

A page-table entry carries a slot and generation. Upload completion must precede its publication, and publication must precede a reader observing it:

$ "write payload" prec "upload completion" prec "publish snapshot" prec "read payload" $

Each arrow requires the backend's applicable execution and memory-visibility guarantees. CPU submission order alone is not a GPU memory barrier. Validate directory ranges, decoded sizes and checksums before publication. A reader matches both virtual identity and generation; a matching generation does not make early slot reuse safe.

== Retirement across queues

Let $u_q(s)$ be the last submission value on queue $q$ that may read slot $s$, and $c_q$ its completed value. Once all future snapshots stop publishing $s$, reuse is allowed only when

#result[
$ forall q: c_q >= u_q(s) $
]

Values from different queue timelines are not numerically comparable. Track each queue or join them with a completion event. Include material resolve, shadows and any temporal pass that dereferences geometry. A previous-frame ID buffer requires the previous token lookup and its referenced geometry to stay valid, or a design that stores only self-contained history values.

Increment the generation on reuse. A $b$-bit generation field wraps after $2^b$ reuses; prove no stale reference can survive that interval, use a sufficiently wide epoch, or force a quiescent reset before wrap. Never rely on a CPU frame-count delay as proof of GPU completion.

== Requests and useful bandwidth

For a requested group define missing bytes over the union of its dependencies, so shared pages are counted once. A possible scheduling score is projected error reduction times affected pixel area divided by incremental bytes. This is a heuristic, not a correctness rule. Add aging and a hard camera-fallback reservation so low-priority shadow demand cannot cause starvation or evict covering camera geometry.

$ A_"io" = B_"read"/B_"useful payload", quad A_"dep"=B_"missing closure"/B_"direct request" $

Report both amplification ratios and distinguish disk read, decoded, uploaded and published bytes. Little's law (§14) describes averages; explicit capacities and admission rules handle bursts.

Source map: streaming references (§7) and Vulkan/Metal ordering contracts (§9 of references).

= Visibility resolve and texture footprints

== Analytic barycentric derivatives

Use the signed edge convention of §10 and screen pixel coordinates. For cyclic $(i,j,k)$ over $(0,1,2)$:

$ partial_x lambda_i=(y_k-y_j)/A, quad partial_y lambda_i=-(x_k-x_j)/A $

These are constant over the projected triangle. Reject a zero-area projection and define a stable fallback for ill-conditioned tiny triangles; dividing by a small signed area without checking can turn one bad triangle into NaNs across a material tile.

Let $r_i=1/w_i$, $D=sum_i lambda_i r_i$, and for a scalar attribute $a$, $N_a=sum_i lambda_i r_i a_i$. Then $a=N_a/D$ and the quotient rule gives

#result[
$ partial_x a=((partial_x N_a)D-N_a(partial_x D))/D^2 $
$ partial_y a=((partial_y N_a)D-N_a(partial_y D))/D^2 $
]

Apply componentwise to UVs. The derivatives are with respect to one pixel, not one NDC unit. Use the same projected triangle, viewport convention and clipping provenance as visibility. Do not finite-difference unrelated neighbouring triangle IDs in the fullscreen resolve.

== Texture-space footprint

For texture dimensions $T_u,T_v$, form

$ bold(J)_"tex"=mat(T_u partial_x u,T_u partial_y u; T_v partial_x v,T_v partial_y v) $

Its singular values are the major and minor footprint stretches in texels per pixel. An isotropic conservative diagnostic is $ell=log_2(max(1,sigma_max(bold(J)_"tex")))$. The ratio of the stretches describes anisotropy when the minor stretch is nonzero. Pass explicit gradients to the sampler; exact filtering and anisotropy limits are backend state, not determined by this diagnostic equation.

Opacity tests in the visibility pass need a consistent footprint and cutoff. Resolve cannot repair pixels already committed as opaque by an incorrect alpha test.

== Tangent frames and mirrored instances

Transform the tangent by $bold(A)$ and the normal by $bold(A)^(-T)$. Orthogonalize and normalize:

$ hat(bold(t))'="normalize"(bold(A)bold(t)-hat(bold(n))'(hat(bold(n))' dot bold(A)bold(t))) $

If the source tangent sign is $h in {-1,1}$, the transformed bitangent is

$ hat(bold(b))'=h "sign"(det(bold(A))) (hat(bold(n))' times hat(bold(t))') $

Decode a tangent-space normal through this frame, then normalize. Handle a collapsed tangent explicitly. Mirror parity also changes geometric winding; set raster front-face state consistently with the intended outward orientation.

Source map: Burns–Hunt, Hable and PBRT (§§5–6 of references); glTF tangent conventions (§9).

= Lighting, cascades and bounded deformation

== A minimal opaque BRDF contract

All colours in the lighting computation are linear. Let unit $bold(n),bold(l),bold(v)$ be the shading normal, direction to light and direction to eye, and $bold(h)="normalize"(bold(l)+bold(v))$. For positive $N_l=bold(n) dot bold(l)$ and $N_v=bold(n) dot bold(v)$:

$ f_s = (D_"GGX" G F)/(4 N_l N_v) $

With perceptual roughness $r$, choose and freeze $alpha=max(r^2,alpha_min)$, where $alpha_min>0$ is a documented numerical policy. Then

$ D_"GGX" = alpha^2/(pi ((bold(n) dot bold(h))^2(alpha^2-1)+1)^2 $
$ G_1(x)=2x/(x+sqrt(alpha^2+(1-alpha^2)x^2)), quad G=G_1(N_l)G_1(N_v) $
$ F=F_0+(1-F_0)(1-max(0,bold(v) dot bold(h)))^5 $

This is separable Smith masking with Schlick Fresnel. Correlated Smith and multiple-scattering compensation are different models; match whichever model the chosen reference uses.

For metallic weight $m$ and linear base colour $bold(c)$, a simple baseline is $F_0=(1-m)0.04+m bold(c)$ and $f_d=(1-m)(1-F)bold(c)/pi$. Direct contribution is $(f_d+f_s)L_i N_l$; it is zero outside the valid hemisphere. This is a practical single-scattering approximation, not a complete energy-compensated material model. Do not apply sRGB decoding to roughness, metallic, normal or depth channels.

IBL evaluates the hemisphere integral of the same BRDF against incident radiance. Prefiltered environment levels, the BRDF lookup and roughness parameterization must agree. Verify with constant-white illumination and roughness/metallic sweeps before adding SSAO or tone mapping, which can hide material errors.

== Cascaded directional shadows

For $m$ cascades over positive camera depths $n$ to shadow distance $F_s$, a mixed linear/log split is

$ d_i=eta n(F_s/n)^(i/m)+(1-eta)(n+(F_s-n)i/m), quad i=0,dots.h.c,m $

Here $eta in [0,1]$ is a tuning parameter. $F_s$ is a finite shadow distance even with an infinite camera far plane. Fit each cascade in light space and include off-camera casters that can shadow visible receivers.

If its orthographic extent is $L_x times L_y$ over $N_x times N_y$ texels,

$ e_"shadow" <= s e_"obj" max(N_x/L_x,N_y/L_y) $

A stable projection uses stable extents and snaps the light-space centre to texel steps. Blend neighbouring cascades across a declared overlap; do not let a moving fit change texel scale every frame.

For reversed shadow depth, a receiver is lit when $z_"receiver"+b >= z_"stored"$, for positive receiver bias $b$. An equivalent caster bias moves stored depth down. Too much bias detaches shadows; too little gives self-shadowing. Freeze the depth convention and tune bias with slope and texel scale rather than transferring a standard-Z sign blindly.

== Deformation envelopes

If displacement satisfies $norm(bold(u)(x,t)) <= a$, enlarge a rest sphere radius to $r+a$ before the instance scale bound is applied. Previous-frame culling uses a bound valid at the previous state; swept bounds must cover the whole interval when used for prediction.

A displacement-amplitude bound alone does not preserve simplification accuracy under deformation. If the common deformation map $F_t(x)=x+bold(u)(x,t)$ is Lipschitz with constant $L_F$, then

$ d_H(F_t(S),F_t(S')) <= L_F d_H(S,S') $

For differentiable displacement, $L_F <= 1+sup_x norm(bold(J)_u(x,t))_2$ is sufficient on the relevant domain. Without a spatial regularity bound, a much looser bound is $d_H(F_t(S),F_t(S')) <= d_H(S,S')+2a$. Account for attribute-dependent motion as well. Expanding culling bounds prevents disappearing foliage; it does not certify the declared LOD error.

Source map: Filament and shadow-map references (§12 of references), glTF (§9), and the error-bound arguments of §8.

= Temporal reprojection and fixed-step play

== Motion vectors and jitter

For a current surface point with valid object-space correspondence, compute current and previous clip positions using their respective camera, instance and deformation states. After perspective division and viewport mapping, let unjittered positions be $bold(p)_t$ and $bold(p)_(t-1)$ in pixels. Define motion

$ bold(m)=bold(p)_t-bold(p)_(t-1) $

If $bold(j)_t$ is the pixel shift actually added by jitter and $bold(x)$ is the current jittered sample position, the previous history coordinate is

#result[
$ bold(x)_"history"=bold(x)-bold(m)+bold(j)_(t-1)-bold(j)_t $
]

Alternatively store jitter-inclusive motion and omit the extra correction. Do one or the other. Reject invalid homogeneous divides, out-of-bounds history, camera cuts, incompatible depth/normal history and disocclusions. LOD switching may remove exact surface correspondence: rigid reprojection is an estimate and history confidence must reflect that. Ephemeral triangle tokens alone cannot establish temporal identity.

== History filtering and a measurable Q6

A basic filter is

$ C_t=(1-w_t)C_"current"+w_t "clip"(C_"history",N_t), quad 0 <= w_t < 1 $

where $N_t$ is an explicitly defined current neighbourhood range. Invalid history uses $w_t=0$. Filtering can conceal an error; it does not repair missing coverage or certify the geometric envelope.

For a scripted route, compare against a fixed full-detail reference with matching camera, exposure and temporal settings. On a frozen set of valid surface tracks, define residual luminance $r_t=Y_"LOD",t-Y_"reference",t$ after reprojection and

$ V=(1/(N-1))sum_(t=1)^N(r_t-macron(r))^2 $

Report residual mean or RMS alongside variance, plus percentiles over tracks and the excluded/disoccluded fraction. Low variance alone can conceal a persistent bias or a frozen image. Freeze the numerical threshold on a calibration sequence and evaluate on held-out camera motions. Pixel variance on an unreprojected moving image mostly measures scene motion, not LOD instability.

== Fixed simulation timestep

For simulation step $h$, add elapsed time $Delta t$ to accumulator $a$, execute $k=floor(a/h)$ steps, subtract $k h$, and render between retained states using $alpha=a/h$. Choose and document the interpolation convention and its latency. For deterministic replay, assign input events to simulation ticks and retain seeds and relevant state hashes.

Limit catch-up work so a slow frame does not start an unbounded spiral. Interactive time dropping is a policy choice; benchmark replays must log it or advance a fixed sequence of simulation ticks so identical routes remain comparable. Fixed timestep alone does not guarantee cross-architecture floating-point determinism.

Source map: Karis TAA (§6 of references), Fiedler and temporal validation notes (§12).

= Measurement equations and decision tests

== Explicit quantiles and rare failures

Freeze a quantile estimator. A simple reproducible choice for sorted frame times $t_(1) <= dots.h.c <= t_(N)$ is nearest-rank $q_p=t_(ceil(p N))$, with $0<p<=1$. Different interpolation rules need not agree on short runs.

With independent trials and a true success probability $p$, the chance that all $n$ trials succeed is $p^n$. After zero observed failures, a one-sided 95% lower confidence bound for success is $0.05^(1/n)$. Even 20/20 successful teleports gives only about 0.861, not statistical proof of a 95% population success rate. At least 59 independent successes with zero failures are needed for that lower bound to exceed 0.95. The existing 19/20 gate is an empirical corpus criterion, which is legitimate when labelled that way.

Frame times and repeated teleports are often dependent. Use independent runs or block-resampling that preserves dependence for uncertainty estimates, and retain per-run results. Five repetitions and a percentage threshold do not automatically establish statistical significance.

== Depth and image errors

With $z=n/d$, local error propagation gives

$ |delta d| approx (d^2/n)|delta z| $

A fixed normalized-depth tolerance is therefore a weak far-distance geometry test. Preserve the existing raster-comparison tolerance but also report ULP or relative/view-space depth error with a documented background mask. At $n=0.1$ m, an interval from depth 0.001 to 0.00102 maps from 100 m to about 98.04 m. Coverage and geometric quality need independent checks.

For valid pixel set $M$ and three linear colour channels,

$ "MAE"=(1/(3|M|))sum_(p in M)sum_(c=1)^3|I_(p c)-R_(p c)| $

Also report a worst-case or high-quantile error and exact coverage failures. Perceptual similarity is useful secondary evidence, not a replacement for a zero-hole gate. Declare whether a value of $1/255$ means a normalized linear unit or a quantized code step; they are not interchangeable after a transfer function.

== Critical paths and pass budgets

For a render dependency DAG with duration $t_v$ at pass $v$, an ideal dependency critical path is

$ T_"critical"=max_"paths" sum_(v in "path") t_v $

Resource contention and queue serialization can increase actual elapsed time. Instrument the real completion chain. The sum of per-pass $p_95$ values is not generally the frame $p_95$, and concurrent queue times cannot be added as if serial.

== The compute-rasterizer decision

Let $f$ be the measured fraction of baseline visibility time that a compute path can accelerate, $S$ that part's speedup, and $h$ additional classification/merge cost normalized to baseline time. The total speedup model is

#result[
$ S_"total" = 1/((1-f)+f/S+h) $
]

Even an infinitely fast compute path cannot double total visibility time unless $f >= 0.5+h$. This is a break-even model, not a performance promise. Area or pixel-weighted histograms alone do not estimate $f$: triangles with zero covered samples can still consume setup and culling work. Retain triangle-count, coverage, rejected-work and time/counter distributions, then time controlled ablations.

Source map: NIST binomial reference and Amdahl (§12 of references); original measurements remain NOT RUN.


#pagebreak()

= Quick reference

#set math.equation(numbering: none)

#sheet(
  (auto, 1fr, auto),
  [Quantity], [Expression], [§],

  [Scale bound],
  [$s = sigma_max (bold(A)) = sqrt(lambda_max (bold(A)^T bold(A))) <= norm(bold(A))_F$],
  [2],

  [Screen-space error],
  [$e_"px" approx (e_"obj" dot s dot H) slash (2 tan(theta slash 2) d_"near")$],
  [5],

  [Nearest depth],
  [$d_"near" = max(n,hat(bold(f)) dot (bold(c)-bold(e))-r)$],
  [5],

  [Switch distance],
  [$d_"switch" = (e_"obj" dot s dot H) slash (2 tan(theta slash 2) tau)$],
  [5],

  [Local predicate, with §16 conditions],
  [$e_"px" ("parent") > tau and e_"px" ("node") <= tau$],
  [5],

  [Visibility key],
  [$("bits"(z) << 32) or "token"$, atomic $max$],
  [4],

  [Normal transform],
  [$bold(n)' = (bold(A)^(-1))^T bold(n)$],
  [1],

  [Edge function],
  [$E(x,y) = (x - x_0)(y_1 - y_0) - (y - y_0)(x_1 - x_0)$],
  [10],

  [Perspective-correct],
  [$a = (sum lambda_i a_i slash w_i) slash (sum lambda_i slash w_i)$],
  [10],

  [HZB reduction],
  [$min$ of four texels, reversed Z],
  [11],

  [HZB level],
  [$L = "clamp"(ceil(log_2 max(1,w,h)),0,L_max)$],
  [11],

  [Quadric],
  [$bold(Q) = sum w_bold(p) bold(p) bold(p)^T$, error $tilde(bold(v))^T bold(Q) tilde(bold(v))$],
  [7],

  [DAG total nodes],
  [$N slash (1 - rho) = 2N$ at $rho = 1 slash 2$],
  [13],

  [Quantization error],
  [$e_q <= sqrt(Delta_x^2+Delta_y^2+Delta_z^2)/2$],
  [12],

  [Little's law],
  [$L = lambda W$],
  [14],

  [Percentile sample heuristic],
  [$approx 100 slash (1 - p)$],
  [15],

  [Frame-rate average],
  [harmonic mean $= 1 slash macron(t)$],
  [15],

  [Certified projected error],
  [$e_"px" <= K s e_"obj"$ on the stated domain],
  [5],

  [Conservative Hi-Z rejection],
  [$z_"candidate"^+ + epsilon_z < z_"occluder"^-$],
  [11],

  [Group readiness],
  [$"cl"(P(g)) subset.eq R_f$, plus cut compatibility],
  [17],

  [Safe slot reuse],
  [$forall q: c_q >= u_q(s)$; no future publication],
  [17],

  [Attribute derivative],
  [$partial_x a=((partial_x N_a)D-N_a partial_x D)/D^2$],
  [18],

  [Temporal history coordinate],
  [$bold(x)-bold(m)+bold(j)_(t-1)-bold(j)_t$],
  [20],

  [Hybrid speedup model],
  [$1/((1-f)+f/S+h)$],
  [21],

)
