import copy
import importlib.util
from pathlib import Path
import re
import unittest

spec=importlib.util.spec_from_file_location('routes',Path(__file__).with_name('core_routes.py'))
routes=importlib.util.module_from_spec(spec); spec.loader.exec_module(routes)


def fixture(wedges, budget=65):
    nodes=[]; edges=[]; gateways=[]; sectors=[]
    def edge(a,b): edges.append(dict(a=a,b=b))
    for name,major,sector in wedges:
        local=[]
        def node(suffix,role,lane,ranks,cost):
            n=dict(id=f'Core.{name}.{suffix}',constellation=name,coreRole=role,coreLaneIndex=lane,ranks=ranks,cost=cost,cornerstone=False,prerequisites=[],exclusive=[])
            nodes.append(n);local.append(n);return n
        g=node('Gateway','Gateway',0,1,1);gateways.append(g['id']);sectors.append(dict(name=name,sector=sector))
        notables=[]
        for i in range(3 if major else 2):
            m=node(f'Minor{i}','LaneMinor',i,3,1);m['prerequisites']=[dict(id=g['id'],rank=1)];edge(g['id'],m['id'])
            n=node(f'Notable{i}','LaneNotable',i,1,2);n['prerequisites']=[dict(id=m['id'],rank=1)];edge(m['id'],n['id']);notables.append(n)
        if major:
            for i in range(2):
                n=node(f'Link{i}','Link',i,1,1)
                n['prerequisiteGroups']=[dict(minimumSatisfied=1,candidates=[dict(id=x['id'],rank=1) for x in notables[i:i+2]])]
                edge(notables[i]['id'],n['id']);edge(notables[i+1]['id'],n['id'])
        c=node('Convergence','Convergence',0,1,3 if major else 2)
        c['prerequisites']=[dict(id=g['id'],rank=1)]
        c['prerequisiteGroups']=[dict(minimumSatisfied=2,candidates=[dict(id=x['id'],rank=1) for x in notables])]
        for n in notables:edge(n['id'],c['id'])
        if major:
            k=node('Keystone','Keystone',0,1,5);k['requiredConstellationInvestment']=18
            k['prerequisites']=[dict(id=c['id'],rank=1)];edge(c['id'],k['id'])
    for i,g in enumerate(gateways):edge(g,gateways[(i+1)%len(gateways)])
    return dict(budgets=dict(core=budget),trees=[dict(id='Core.Slice',currency='CorePoints',restrictEntryToOwnedNeighbor=True,
        coreWedgeOrder=[name for name,_,_ in wedges],entryNodes=gateways,constellations=sectors,nodes=nodes,adjacencyEdges=edges)])


class CensusRoutes(unittest.TestCase):
    def test_local_routes_derive_real_costs_and_optional_ranks(self):
        report=routes.measure(fixture([('One',True,'A'),('Two',True,'A'),('Three',True,'B')]))
        self.assertEqual((report['nodes'],report['offered']),(33,78))
        self.assertEqual((report['wedges'][0]['one_lane'],report['wedges'][0]['convergence'],report['wedges'][0]['keystone']),(4,10,23))
        self.assertTrue(report['three_keystones_exceed_budget'])
        self.assertEqual(report['frontiers'][-1]['cost'],56)

    def test_cost_change_changes_measurement_without_roster_constants(self):
        census=fixture([('One',False,'A'),('Two',False,'B'),('Three',False,'C')],budget=10)
        first=routes.measure(census)
        census['trees'][0]['nodes'][0]['cost']=2
        changed=routes.measure(census)
        self.assertEqual(changed['offered'],first['offered']+1)
        self.assertEqual(changed['wedges'][0]['one_lane'],5)
        self.assertEqual(changed['frontiers'][0]['cost'],4)

    def test_rank_requirement_changes_actual_lane_price(self):
        census=fixture([('One',False,'A'),('Two',False,'B'),('Three',False,'C')])
        for node in census['trees'][0]['nodes']:
            if node['constellation']=='One' and node['coreRole']=='LaneNotable':node['prerequisites'][0]['rank']=3
        report=routes.measure(census)
        self.assertEqual(report['wedges'][0]['one_lane'],6)
        self.assertEqual(report['wedges'][0]['convergence'],13)

    def test_legacy_missing_roles_cross_links_and_global_gates_refused(self):
        base=fixture([('One',True,'A'),('Two',False,'B'),('Three',False,'C')])
        cases=[]
        legacy=copy.deepcopy(base);legacy['trees'][0]['restrictEntryToOwnedNeighbor']=False;cases.append(legacy)
        missing=copy.deepcopy(base);missing['trees'][0]['nodes'][0].pop('coreRole');cases.append(missing)
        cross=copy.deepcopy(base);cross['trees'][0]['adjacencyEdges'].append(dict(a='Core.One.Minor0',b='Core.Two.Minor0'));cases.append(cross)
        gate=copy.deepcopy(base);gate['trees'][0]['nodes'][1]['requiredTreeInvestment']=3;cases.append(gate)
        for case in cases:
            with self.assertRaises(routes.RouteError):routes.measure(case)

    def test_unreachable_group_refused(self):
        census=fixture([('One',True,'A'),('Two',False,'B'),('Three',False,'C')])
        n=next(n for n in census['trees'][0]['nodes'] if n['id']=='Core.One.Convergence')
        n['prerequisiteGroups'][0]['minimumSatisfied']=4
        with self.assertRaisesRegex(routes.RouteError,'group'):routes.measure(census)


if __name__=='__main__':unittest.main()
