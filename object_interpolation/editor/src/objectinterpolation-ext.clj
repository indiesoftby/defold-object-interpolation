;; Copyright 2025 Indiesoft LLC
;; Licensed under the MIT License.

(ns editor.defold-objectinterpolation
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.graph-util :as gu]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms :as protobuf-forms]
            [editor.resource-node :as resource-node]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace]))

(set! *warn-on-reflection* true)

(def ^:private objectinterpolation-ext "objectinterpolation")
(def ^:private objectinterpolation-icon "/object_interpolation/editor/resources/icons/Icon-ObjectInterpolation.png")
(def ^:private objectinterpolation-template "/object_interpolation/editor/resources/templates/template.objectinterpolation")

(def ^:private objectinterpolation-plugin-desc-cls
  (delay (workspace/load-class! "com.dynamo.objectinterpolation.proto.ObjectInterpolation$ObjectInterpolationDesc")))

;; /////////////////////////////////////////////////////////////////////////////////////////////

;; Helper function for property validation. Applies the supplied validate-fn to
;; the value and the property name, and returns an ErrorValue in case it returns
;; a non-nil string expressing a problem with the value.
(defn- validate-property [prop-kw validate-fn node-id value]
  (validation/prop-error :fatal node-id prop-kw validate-fn value (validation/keyword->name prop-kw)))

;; Property validate-fn for use with the validate-property function. Takes a
;; property value and the name of the property. Is expected to test the validity
;; of the property value, and return a string describing the problem in case the
;; value is not valid. For valid values, it should return nil.
(defn- prop-empty? [value prop-name]
  (when (empty? value)
    (format "'%s' must be specified" prop-name)))

;; These all validate a single property and produce a human-readable error
;; message if the value is invalid. They are used for validation in the
;; Property panel, and to produce build errors when building the project.
(defn- validate-target-object [node-id value]
  ;; No validation yet
  nil)

(defn- validate-apply-transform [node-id value]
  ;; No validation needed for enum values - they're constrained by the dropdown
  nil)

;; Build function embedded in the build targets for ObjectInterpolation. Once build
;; targets have been gathered, this function will be called with a BuildResource
;; (for output), and the user-data from a ObjectInterpolation build target produced by
;; the produce-build-targets defnk. It's expected to return a map containing the
;; BuildResource and the content that should be written to it as a byte array.
(defn- build-objectinterpolation [resource _dep-resources user-data]
  (let [objectinterpolation-pb (:objectinterpolation-pb user-data)
        content (protobuf/map->bytes @objectinterpolation-plugin-desc-cls objectinterpolation-pb)]
    {:resource resource
     :content content}))

;; Produce the build targets for a single ObjectInterpolation resource. Each ObjectInterpolation
;; resource results in one binary resource for the engine runtime. The contents
;; of the build target are hashed and used to determine if we need to re-run the
;; build-fn and write a new file. If there are build errors, return an
;; ErrorValue that will abort the build and report the errors to the user.
(g/defnk produce-build-targets [_node-id resource save-value own-build-errors]
  (g/precluding-errors own-build-errors
    [(bt/with-content-hash
       {:node-id _node-id
        :resource (workspace/make-build-resource resource)
        :build-fn build-objectinterpolation
        :user-data {:objectinterpolation-pb save-value}})]))

;; Callback invoked by the form view when a value is edited by the user. Is
;; expected to return a sequence of transaction steps that perform the relevant
;; changes to the graph. In our case, we simply set the value of the property on
;; the edited ObjectInterpolationNode.
(defn- set-form-op [user-data property-path value]
  (assert (= 1 (count property-path)))
  (let [node-id (:node-id user-data)
        prop-kw (first property-path)]
    (g/set-property node-id prop-kw value)))

;; Callback invoked by the form view when a value is cleared (or reset), by the
;; user. Is expected to return a sequence of transaction steps that perform the
;; relevant changes to the graph. In our case, we simply clear the value of the
;; property on the edited ObjectInterpolationNode.
(defn- clear-form-op [user-data property-path]
  (assert (= 1 (count property-path)))
  (let [node-id (:node-id user-data)
        prop-kw (first property-path)]
    (g/clear-property node-id prop-kw)))

;; Produce form-data for editing ObjectInterpolation using the form view. This can be
;; used to open standalone ObjectInterpolation resources in an editor tab. The form view
;; will render a simple user-interface based on the data we return here.
(g/defnk produce-form-data [_node-id apply-transform target-object]
  {:navigation false
   :form-ops {:user-data {:node-id _node-id}
              :set set-form-op
              :clear clear-form-op}
   :sections [{:title "Object Interpolation"
               :fields [{:path [:apply-transform]
                         :label "Apply Transform"
                         :type :choicebox
                         :options (protobuf-forms/make-enum-options (workspace/load-class! "com.dynamo.objectinterpolation.proto.ObjectInterpolation$ObjectInterpolationDesc$ApplyTransform"))
                         :default (ffirst (protobuf/enum-values (workspace/load-class! "com.dynamo.objectinterpolation.proto.ObjectInterpolation$ObjectInterpolationDesc$ApplyTransform")))}
                        {:path [:target-object]
                         :label "Target Object"
                         :type :string
                         :default ""}]}]
   :values {[:apply-transform] apply-transform
            [:target-object] target-object}})

;; Produce a Clojure map representation of the protobuf field values that can be
;; saved to disk in protobuf text format, or built into a binary protobuf
;; message for the engine runtime. To keep the project files small, we omit
;; default values from the output.
(g/defnk produce-save-value [apply-transform target-object]
  (protobuf/make-map-without-defaults @objectinterpolation-plugin-desc-cls
    :apply-transform apply-transform
    :target-object target-object))

;; Produce an ErrorPackage of one or more ErrorValues that express problems with
;; our ObjectInterpolation. If there are no errors, produce nil. Any errors produced here
;; will be reported as clickable errors in the Build Errors view.
(g/defnk produce-own-build-errors [_node-id apply-transform target-object]
  (g/package-errors
    _node-id
    (validate-apply-transform _node-id apply-transform)
    (validate-target-object _node-id target-object)))

;; After a ObjectInterpolationNode has been created for our ObjectInterpolation resource, this
;; function is called with our self node-id and a Clojure map representation of
;; the protobuf data read from our resource. We're expected to return a sequence
;; of transaction steps that populate our ObjectInterpolationNode from the protobuf data.
;; In our case, that simply means setting the property values on our node to the
;; values from the protobuf data.
(defn- load-objectinterpolation [_project self _resource data]
  (gu/set-properties-from-pb-map self @objectinterpolation-plugin-desc-cls data
    apply-transform :apply-transform
    target-object :target-object))

;; Defines a node type that will represent ObjectInterpolation resources in the graph.
;; Whenever we encounter a .objectinterpolation file in the project, a ObjectInterpolationNode is
;; created for it, and the load-fn we register for the resource type will be run
;; to populate the ObjectInterpolationNode from the protobuf data. We implement a series
;; of named outputs to make it possible to edit the node using the form view,
;; save changes, build binaries for the engine runtime, and so on.
(g/defnode ObjectInterpolationNode
  (inherits resource-node/ResourceNode)

  ;; Editable properties.
  ;; The defaults should be equal to the ones in the ObjectInterpolation$ObjectInterpolationDesc
  ;; class generated from `objectinterpolation_ddf.proto`. This ensures we'll have the
  ;; correct defaults for fields not present in the `.objectinterpolation` files.
  (property apply-transform g/Keyword
            (default :apply-transform-none)
            (dynamic edit-type (g/constantly (properties/->pb-choicebox (workspace/load-class! "com.dynamo.objectinterpolation.proto.ObjectInterpolation$ObjectInterpolationDesc$ApplyTransform"))))
            (dynamic error (g/fnk [_node-id apply-transform] (validate-apply-transform _node-id apply-transform))))

  (property target-object g/Str
            (default (protobuf/default @objectinterpolation-plugin-desc-cls :target-object))
            (dynamic edit-type (g/constantly {:type g/Str :script-property-type :script-property-type-hash}))
            (dynamic read-only? (g/fnk [apply-transform] (not= :apply-transform-target apply-transform)))
            (dynamic error (g/fnk [_node-id target-object] (validate-target-object _node-id target-object))))

  ;; Outputs we're expected to implement.
  (output form-data g/Any :cached produce-form-data)
  (output save-value g/Any :cached produce-save-value)
  (output own-build-errors g/Any produce-own-build-errors)
  (output build-targets g/Any :cached produce-build-targets))

;; /////////////////////////////////////////////////////////////////////////////////////////////

;; Register our .objectinterpolation resource type with the workspace. Whenever we find a
;; .objectinterpolation file in the project, a ObjectInterpolationNode is created for it. Then,
;; the load-fn populates the ObjectInterpolationNode from a Clojure map representation of
;; the protobuf data we load from the .objectinterpolation resource. When we register our
;; resource type, we also tag ourselves as a component that can be used in game
;; objects, and declare which view types can present our resource for editing in
;; an editor tab. In our case, we will use the form view for editing. To work
;; with the form view, our node is expected to implement the form-data output
;; required by the form view.
(defn- register-resource-types [workspace]
  (resource-node/register-ddf-resource-type
    workspace
    :ext objectinterpolation-ext
    :label "Object Interpolation"
    :node-type ObjectInterpolationNode
    :ddf-type @objectinterpolation-plugin-desc-cls
    :load-fn load-objectinterpolation
    :icon objectinterpolation-icon
    :view-types [:cljfx-form-view :text]
    :view-opts {}
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{}}}
    :template objectinterpolation-template))

;; The plugin
(defn- load-plugin-objectinterpolation [workspace]
  (g/transact (register-resource-types workspace)))

(defn- return-plugin []
  (fn [workspace]
    (load-plugin-objectinterpolation workspace)))

(return-plugin)
